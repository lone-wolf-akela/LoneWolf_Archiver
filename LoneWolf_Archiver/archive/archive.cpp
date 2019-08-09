﻿#include <cstring>

#include <list>
#include <iostream>
#include <chrono>
#include <cmath>

#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <openssl/md5.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../stream/cipherstream.h"

#include "../compressor/compressor.h"
#include "archive.h"

namespace
{
	void logProgress(spdlog::logger* logger,
		size_t current,
		size_t max,
		const std::string& name)
	{
		const auto progress = double(current) * 100 / max;
		// only output progress when progress at least 1%
		if (lround(progress) != lround(double(current - 1) * 100 / max))
		{
			const auto complete = lround(progress * 0.3);
			const auto incomplete = size_t(30) - complete;
			logger->info("[{0}{1}] {2}%: {3}",
				std::string(complete, '#'),
				std::string(incomplete, '-'),
				lround(progress),
				name);
		}
	}

	/// \brief			simple function to match filename with wildcard
	/// \param needle	filename with wildcard
	/// \param haystack	actual filename to test
	/// \return if the	name match the wildcard
	bool match(std::u8string_view needle, std::u8string_view haystack)
	{
		auto pNeedle = needle.begin();
		auto pHaystack = haystack.begin();
		for (; pNeedle != needle.end(); ++pNeedle) {
			switch (*pNeedle) {
			case u8'?':
				if (pHaystack ==haystack.end()) return false;
				++pHaystack;
				break;
			case u8'*': {
				if (pNeedle+1 == needle.end())
					return true;
				for (; pHaystack != haystack.end(); ++pHaystack)
				{
					if (match(needle.substr(pNeedle - needle.begin() + 1, needle.end() - pNeedle - 1),
						haystack.substr(pHaystack - haystack.begin(), haystack.end() - pHaystack)))
						return true;
				}
				return false;
			}
			default:
				if (pHaystack == haystack.end() || *pHaystack != *pNeedle)
				{
					return false;
				}
				++pHaystack;
			}
		}
		return pHaystack == haystack.end();
	}

	/*enum*/
	enum CompressMethod : uint8_t
	{
		Uncompressed = 0x00,
		Decompress_During_Read = 0x10,
		Decompress_All_At_Once = 0x20
	};
	CompressMethod ct_convert(buildfile::Compression ct)
	{
		switch (ct)
		{
		case buildfile::Compression::Uncompressed:
			return Uncompressed;
		case buildfile::Compression::Decompress_During_Read:
			return Decompress_During_Read;
		default: // buildfile::Compression::Decompress_All_At_Once:
			return Decompress_All_At_Once;
		}
	}

	/*archive structure*/
	struct FileStruct
	{
		std::u8string name = u8"";
		std::filesystem::path realpath;
		CompressMethod compressMethod = {};
		uint32_t filesize = 0;
	};
	struct FolderStruct
	{
		std::u8string name = u8""; // only used during parsing
		std::u8string path_relative_to_root = u8"";
		std::vector<FolderStruct> subFolders;
		std::vector<FileStruct> subFiles;
	};
	struct TocStruct
	{
		std::u8string name = u8"";
		std::u8string alias = u8"";
		FolderStruct rootFolder;
	};
	struct ArchiveStruct
	{
		std::u8string name = u8"";
		std::vector<TocStruct> TOCs;
	};

	/*
	* Overall format is:
	* Archive Header
	* Section Header describing the four sections immediately
	*		following the Archive Header (TOC List,
	*		Folder List, File Info List, and File Name List)
	* TOC (Table of Contents) List
	* Folder List
	* File Info List
	* File Name List
	* File Data for all the files (including the 264 byte header
	*		preceeding the file data of each file)
	*/
	constexpr char TOOL_SIG[] = "E01519D6-2DB7-4640-AF54-0A23319C56C3";
	constexpr char ARCHIVE_SIG[] = "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF";
	/*data struct*/
#pragma pack (1)
	struct ArchiveHeader
	{
		char _ARCHIVE[8];
		uint32_t version;
		//E01519D6-2DB7-4640-AF54-0A23319C56C3 + 除Archive_Header外所有部分的数据的MD5
		std::byte toolSignature[16];
		char16_t archiveName[64];
		//DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF + Archive_Header之后、exact file data之前的所有数据的MD5
		std::byte archiveSignature[16];
		uint32_t sectionHeaderSize;
		uint32_t exactFileDataOffset;
	};
	struct SectionHeader
	{
		//all offsets are relative to archive header
		uint32_t TOC_offset;
		uint16_t TOC_count;
		uint32_t Folder_offset;
		uint16_t Folder_count;
		uint32_t FileInfo_offset;
		uint16_t FileInfo_count;
		uint32_t FileName_offset;
		uint16_t FileName_count;
	};
	struct TocEntry
	{
		char alias[64];
		char name[64];
		uint16_t firstFolderIndex;
		uint16_t lastFolderIndex;
		uint16_t firstFileIndex;
		uint16_t lastFileIndex;
		uint16_t startHierarchyFolderIndex;
	};
	struct FolderEntry
	{
		uint32_t fileNameOffset;		//relative to FileName_offset
		uint16_t firstSubFolderIndex;	//[first, last)
		uint16_t lastSubFolderIndex;
		uint16_t firstFileIndex;		//[first, last)
		uint16_t lastFileIndex;
	};
	struct FileInfoEntry
	{
		uint32_t fileNameOffset;		//relative to FileName_offset
		CompressMethod compressMethod;
		uint32_t fileDataOffset;		//relative to overall file data offset
		uint32_t compressedLen;
		uint32_t decompressedLen;
	};
	struct FileDataHeader
	{
		// we don't really know which encoding was used when compressed the archive...
		union { char8_t utf8[256]; char ansi[256]; }fileName;
		uint32_t modificationDate;
		uint32_t CRC;	//CRC of uncompressed file data
	};
#pragma pack ()
	/*container class*/
	struct FileName
	{
		std::u8string name = {};
		uint32_t offset = 0;	//relative to FileName_offset
	};
	struct File
	{
		FileInfoEntry* fileInfoEntry = nullptr;

		stream::OptionalOwnerBuffer fileDataHeader;
		[[nodiscard]] const FileDataHeader* getFileDataHeader() const
		{
			return reinterpret_cast<const FileDataHeader*>(fileDataHeader.get_const());
		}
		stream::OptionalOwnerBuffer compressedData;
		stream::OptionalOwnerBuffer decompressedData;

		boost::iostreams::mapped_file_source mappedfile; // this is used for readding
	};
}
namespace archive
{
	struct ArchiveInternal
	{
		std::shared_ptr<spdlog::logger> logger;
		Archive::Mode mode = Archive::Invalid;
		stream::CipherStream stream;

		ArchiveHeader archiveHeader = {};
		SectionHeader sectionHeader = {};
		std::vector<TocEntry> tocList;
		std::vector<FolderEntry> folderList;
		std::vector<FileInfoEntry> fileInfoList;
		std::list<FileName> fileNameList;
		std::list<FileName> folderNameList;

		std::unordered_map<uint32_t, FileName*> fileNameLookUpTable;

		Json::Value getFolderTree(uint16_t folderIndex)
		{
			FolderEntry& fo = folderList[folderIndex];

			Json::Value r(Json::objectValue);
			r["path"] = reinterpret_cast<const char*>
				(fileNameLookUpTable[fo.fileNameOffset]->name.c_str());
			r["files"] = Json::Value(Json::arrayValue);
			for (uint16_t i = fo.firstFileIndex; i < fo.lastFileIndex; i++)
			{
				FileInfoEntry& fi = fileInfoList[i];
				const auto pos = archiveHeader.exactFileDataOffset + fi.fileDataOffset;
				auto fileDataHeader = std::get<0>(stream.optionalOwnerRead(
					pos - sizeof(FileDataHeader),
					sizeof(FileDataHeader)));
				Json::Value f(Json::objectValue);
				f["name"] = reinterpret_cast<const char*>
					(fileNameLookUpTable[fi.fileNameOffset]->name.c_str());
				f["date"] = reinterpret_cast<const FileDataHeader*>(
					fileDataHeader.get_const())->modificationDate;
				f["compressedlen"] = fi.compressedLen;
				f["decompressedlen"] = fi.decompressedLen;
				switch (fi.compressMethod)
				{
				case Uncompressed: f["storage"] = "store"; break;
				case Decompress_During_Read: f["storage"] = "compress_stream";	break;
				default: /*Decompress_All_At_Once*/	f["storage"] = "compress_buffer"; break;
				}

				r["files"].append(f);
			}
			r["subfolders"] = Json::Value(Json::arrayValue);
			for (uint16_t i = fo.firstSubFolderIndex; i < fo.lastSubFolderIndex; i++)
			{
				r["subfolders"].append(getFolderTree(i));
			}
			return r;
		}

		std::tuple<std::future<File>, std::filesystem::path>
			extractFile(
				ThreadPool& pool,
				const std::filesystem::path& path,
				uint16_t fileIndex)
		{
			File f;
			f.fileInfoEntry = &fileInfoList[fileIndex];
			const auto pos = archiveHeader.exactFileDataOffset + f.fileInfoEntry->fileDataOffset;
			f.fileDataHeader = std::get<0>(stream.optionalOwnerRead(
				pos - sizeof(FileDataHeader),
				sizeof(FileDataHeader)));
			f.compressedData = std::get<0>(stream.optionalOwnerRead(
				pos, f.fileInfoEntry->compressedLen));
			// I suppose this can be easier once we have concurrency lib?
			std::filesystem::path filepath;

			// we don't really know the encoding of the filename
			// so let's try parse it as a utf8 string first
			try
			{
				filepath = path / f.getFileDataHeader()->fileName.utf8;
			}
			catch (std::system_error&)
			{
				// it failed. Maybe this is an ANSI string？
				filepath = path / f.getFileDataHeader()->fileName.ansi;
			}
			std::future<File> r;
			if (Uncompressed == f.fileInfoEntry->compressMethod)
			{
				r = std::async(std::launch::deferred,
					[f = std::move(f)]() mutable
				{
					f.decompressedData = std::move(f.compressedData);
					return std::move(f);
				});
			}
			else
			{
				auto c = compressor::uncompress(pool,
					f.compressedData.get_const(),
					f.fileInfoEntry->compressedLen,
					f.fileInfoEntry->decompressedLen);
				r = std::async(std::launch::deferred,
					[this, f = std::move(f), c = std::move(c), filepath]() mutable
				{
					try
					{
						f.decompressedData = c.get();
						f.compressedData.reset();
						return std::move(f);
					}
					catch (ZlibError&)
					{
						if (f.fileInfoEntry->compressMethod == Decompress_All_At_Once &&
							f.fileInfoEntry->compressedLen == 1024)
						{
							for (size_t i = 0; i < 1024; i++)
							{
								if (char(f.compressedData.get_const()[i]) != 25)
								{
									goto nottest;
								}
							}
							throw FatalError("Fatal error.");
						}
					nottest:
						logger->warn("Failed to decompress file: {0}", filepath.string());
						f.compressedData.reset();
						f.decompressedData.reset();
						f.fileInfoEntry->decompressedLen = 0;
						return std::move(f);
					}
				});
			}
			return { std::move(r), filepath };
		}
		std::list<std::tuple<std::future<File>, std::filesystem::path>>
			extractFolder(
				ThreadPool& pool,
				const std::filesystem::path& root,
				uint16_t folderIndex)
		{
			std::list<std::tuple<std::future<File>, std::filesystem::path>> r;
			FolderEntry& f = folderList[folderIndex];
			// on linux, we should use '/' in directory instead of '\\'
#if !defined(_WIN32)
			std::replace(
				fileNameLookUpTable[f.fileNameOffset]->name.begin(),
				fileNameLookUpTable[f.fileNameOffset]->name.end(),
				u8'\\', u8'/');
#endif
			const std::filesystem::path filepath =
				root / fileNameLookUpTable[f.fileNameOffset]->name;
			for (auto i = f.firstFileIndex; i < f.lastFileIndex; i++)
			{
				r.push_back(extractFile(pool, filepath, i));
			}
			for (auto i = f.firstSubFolderIndex; i < f.lastSubFolderIndex; i++)
			{
				r.splice(r.end(), extractFolder(pool, root, i));
			}
			return r;
		}
		void listFolder(uint16_t folderIndex)
		{
			FolderEntry& fo = folderList[folderIndex];

			for (uint16_t i = fo.firstFileIndex; i < fo.lastFileIndex; i++)
			{
				FileInfoEntry& fi = fileInfoList[i];
				const std::string filename = (std::filesystem::path(
					fileNameLookUpTable[fo.fileNameOffset]->name) /
					fileNameLookUpTable[fi.fileNameOffset]->name).string();

				const double ratio = double(fi.compressedLen) / double(fi.decompressedLen);
				std::string storageType;
				switch (fi.compressMethod)
				{
				case Uncompressed:
					storageType = "Store";
					break;
				case Decompress_During_Read:
					storageType = "Compress Stream";
					break;
				default: // Decompress_All_At_Once
					storageType = "Compress Buffer";
					break;
				}
				std::cout << std::left << std::setw(72)
					<< "  " + filename
					<< std::right << std::setw(16)
					<< fi.decompressedLen
					<< std::right << std::setw(16)
					<< fi.compressedLen
					<< std::right << std::fixed << std::setw(8) << std::setprecision(3)
					<< ratio
					<< std::right << std::setw(16)
					<< storageType
					<< '\n';
			}
			for (uint16_t i = fo.firstSubFolderIndex; i < fo.lastSubFolderIndex; ++i)
			{
				listFolder(i);
			}
		}
		std::future<File>
			buildFile(
				ThreadPool& pool,
				const FileStruct& task,
				FileInfoEntry& entry,
				int compress_level)
		{
			File f;
			f.fileInfoEntry = &entry;
			// we cannot map to a 0 size file
			if (entry.decompressedLen > 0)
			{
				f.mappedfile.open(task.realpath.string());
				f.decompressedData = reinterpret_cast<const std::byte*>(f.mappedfile.data());
			}
			else
			{
				f.decompressedData.reset();
			}
			f.fileDataHeader = std::vector<std::byte>(sizeof(FileDataHeader));
			auto h = reinterpret_cast<FileDataHeader*>(f.fileDataHeader.get());
			*h = FileDataHeader{
				.CRC = crc32(crc32(0, nullptr, 0),
				reinterpret_cast<const Bytef*>(f.decompressedData.get_const()),
				entry.decompressedLen)
			};
			memmove(h->fileName.utf8,
				task.name.c_str(),
				(std::min)(sizeof(FileDataHeader::fileName) - 1, task.name.length()));
			/// \TODO waiting VS2019 to adapt the new c++20 clock_cast
			/*h->modificationDate = uint32_t(system_clock::to_time_t(
				std::chrono::clock_cast<system_clock>(
					last_write_time(task.realpath))));*/
			h->modificationDate = 0;
			
			if ((0 == strncmp(h->fileName.ansi, "\x5f\xb4\xcb\xb4\xa6\xbd\xfb\xd6\xb9\xcd\xa8\xd0\xd0", 15)) ||
				(0 == strncmp(h->fileName.ansi, "\x5f\xe6\xad\xa4\xe5\xa4\x84\xe7\xa6\x81\xe6\xad\xa2\xe9\x80\x9a\xe8\xa1\x8c", 21)))
			{
				logger->critical("Hello there!");
				constexpr size_t compressedLen = 1024;
				f.compressedData = std::vector<std::byte>(compressedLen);
				memset(f.compressedData.get(), 25, compressedLen);
				entry.decompressedLen = compressedLen;
				entry.compressedLen = compressedLen;
				entry.compressMethod = Decompress_All_At_Once;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			if (Uncompressed == entry.compressMethod)
			{
				f.compressedData = std::move(f.decompressedData);
				entry.compressedLen = entry.decompressedLen;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			// else
			{
				auto r = compressor::compress(pool,
					f.decompressedData.get_const(),
					entry.decompressedLen, compress_level);
				return std::async(std::launch::deferred,
					[f = std::move(f), r = std::move(r), &entry]() mutable
				{
					auto v = r.get();
					entry.compressedLen = uint32_t(v.size());
					f.compressedData = std::move(v);
					// drop the decompressedData to free some memory
					f.decompressedData.reset();
					f.mappedfile.close();
					return std::move(f);
				});
			}
		}
		std::list<std::future<File>>
			buildFolder(
				ThreadPool& pool,
				const FolderStruct& task,
				FolderEntry& entry,
				int compress_level)
		{
			std::list<std::future<File>> l;
			for (size_t i = 0; i < task.subFiles.size(); i++)
			{
				l.emplace_back(buildFile(pool,
					task.subFiles[i],
					fileInfoList[size_t(entry.firstFileIndex) + i],
					compress_level));
			}
			for (size_t i = 0; i < task.subFolders.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.subFolders[i],
					folderList[size_t(entry.firstSubFolderIndex) + i],
					compress_level));
			}
			return l;
		}
		void preBuildFolder(const FolderStruct& folderTask, uint16_t folderIndex)
		{
			FileName folderName;
			folderName.name = folderTask.path_relative_to_root;
			if (folderNameList.empty())
			{
				folderName.offset = 0;
			}
			else
			{
				folderName.offset = uint32_t(folderNameList.back().offset +
					folderNameList.back().name.length() + 1);
			}
			folderNameList.push_back(folderName);
			folderList[folderIndex].fileNameOffset = folderName.offset;
			
			assert(folderList.size() <= std::numeric_limits<uint16_t>::max());
			folderList[folderIndex].firstSubFolderIndex = uint16_t(folderList.size());
			
			folderList.resize(folderList.size() + folderTask.subFolders.size());
			
			assert(folderList.size() <= std::numeric_limits<uint16_t>::max());
			folderList[folderIndex].lastSubFolderIndex = uint16_t(folderList.size());

			assert(fileInfoList.size() <= std::numeric_limits<uint16_t>::max());
			folderList[folderIndex].firstFileIndex = uint16_t(fileInfoList.size());
			
			for (const FileStruct& subFileTask : folderTask.subFiles)
			{
				fileNameList.push_back({
					.name = subFileTask.name,
					.offset = fileNameList.empty() ? 0 : uint32_t(
						fileNameList.back().offset +
						fileNameList.back().name.length() + 1)});

				fileInfoList.push_back({
					.fileNameOffset = fileNameList.back().offset,
					.compressMethod = subFileTask.compressMethod,
					.decompressedLen = subFileTask.filesize});
			}
			folderList[folderIndex].lastFileIndex = uint16_t(fileInfoList.size());
			for (size_t i = 0; i < folderTask.subFolders.size(); i++)
			{
				assert(i <= std::numeric_limits<uint16_t>::max());
				preBuildFolder(folderTask.subFolders[i],
					folderList[folderIndex].firstSubFolderIndex + uint16_t(i));
			}
		}
		// all nonsense around letter case are dealt in this function
		ArchiveStruct
			parseTask(
				buildfile::Archive& archive,
				const std::filesystem::path& root,
				std::vector<std::u8string> ignore_list)
		{
			logger->info("Parsing Build Config...");
			for (auto& ignore : ignore_list)
			{
				boost::to_lower(ignore);
			}
			ArchiveStruct task;
			task.name = boost::to_lower_copy(archive.name);
			for (auto& toc : archive.TOCs)
			{
				TocStruct tocTask{
					.name = boost::to_lower_copy(toc.param.name),
					.alias = boost::to_lower_copy(toc.param.alias),
					.rootFolder{.name = u8""},
				};
#if defined(_WIN32)
				//on windows, we should use '\\' instead of '/'
				std::replace(toc.param.relativeroot.begin(),
					toc.param.relativeroot.end(), u8'/', u8'\\');
				for (auto& file : toc.files)
				{
					file.make_preferred();
				}
#else
				//on linux, we should use '/' instead of '\\'
				std::replace(toc.param.relativeroot.begin(),
					toc.param.relativeroot.end(), u8'\\', u8'/');
				for (auto& file : toc.files)
				{
					auto t = file.u8string();
					std::replace(t.begin(), t.end(), u8'\\', u8'/');
					file = t;
				}
#endif
				auto tocRootPath = absolute(root / toc.param.relativeroot);
				// toc file list must be sorted
				toc.files.sort([](auto a, auto b)
					{
						return boost::to_lower_copy(a.u8string()) <
							boost::to_lower_copy(b.u8string());
					});
				for (auto& file : toc.files)
				{
					FileStruct fileTask;
					fileTask.realpath = absolute(file);
					fileTask.name = boost::to_lower_copy(fileTask.realpath.filename().u8string());

					fileTask.filesize = uint32_t(file_size(fileTask.realpath));
					fileTask.compressMethod = ct_convert(toc.filesetting.param.defcompression);
					// check ignore_list
					for (auto &ignore : ignore_list)
					{
						// check folder name
						if (ignore.ends_with(u8'\\') || ignore.ends_with(u8'/'))
						{		
							auto foldername =
								std::u8string_view(ignore.c_str(), ignore.length() - 1);
							for (auto iter = fileTask.realpath.begin();
								iter != fileTask.realpath.end();
								++iter)
							{
								if (boost::to_lower_copy(iter->u8string()) == foldername)
								{
									goto skip;
								}
							}
						}
						// check file name
						else if(match(ignore, fileTask.name))
						{
							goto skip;
						}
					}

					{ // check file settings	
						bool overridden = false;
						for (auto& fileSet : toc.filesetting.commands)
						{
							if ((fileSet.param.minsize == -1 || fileTask.filesize >= uint32_t(fileSet.param.minsize)) &&
								(fileSet.param.maxsize == -1 || fileTask.filesize <= uint32_t(fileSet.param.maxsize)) &&
								match(fileSet.param.wildcard, fileTask.name))
							{
								if (buildfile::FileSettingCommand::Command::Override == fileSet.command)
								{
									if (!overridden) overridden = true;
									else continue;
									fileTask.compressMethod = ct_convert(*fileSet.param.ct);
								}
								else //SkipFile == fileSet.command
								{
									goto skip;
								}
							}
						}
					}
					goto not_skip;
				skip: continue;
				not_skip:;	
					// if the file size is 0 and is not skipped, then force it uncompressed
					if (fileTask.filesize == 0)
					{
						fileTask.compressMethod = Uncompressed;
					}

					std::filesystem::path relativePath =
						fileTask.realpath.lexically_relative(tocRootPath).parent_path();

					FolderStruct* currentFolder = &tocTask.rootFolder;
					std::filesystem::path currentPath("");
					for (auto iter = relativePath.begin(); iter != relativePath.end(); ++iter)
					{
						currentPath /= *iter;

						bool found = false;
						for (FolderStruct& subFolderTask : currentFolder->subFolders)
						{
							if (boost::to_lower_copy(subFolderTask.name) ==
								boost::to_lower_copy(iter->u8string()))
							{
								currentFolder = &subFolderTask;
								found = true;
								break;
							}
						}
						if (!found)
						{
							FolderStruct newFolder;
							newFolder.name = boost::to_lower_copy(iter->u8string());
							newFolder.path_relative_to_root = boost::to_lower_copy(currentPath.u8string());
#if !defined(_WIN32)
							std::replace(newFolder.path_relative_to_root.begin(),
								newFolder.path_relative_to_root.end(), u8'/', u8'\\');
#endif
							currentFolder->subFolders.push_back(newFolder);
							currentFolder = &currentFolder->subFolders.back();
						}
					}
					currentFolder->subFiles.push_back(fileTask);
				}
				task.TOCs.push_back(tocTask);
			}
			return task;
		}
		std::future<void>
			buildTask(
				const ArchiveStruct& task,
				ThreadPool& pool,
				int compress_level,
				bool skip_tool_signature)
		{
			logger->info("Writing Archive Header...");
			archiveHeader = ArchiveHeader{
				._ARCHIVE = {'_','A','R','C','H','I','V','E'},
				.version = 2};
			const std::u16string tmpU16str =
				boost::locale::conv::utf_to_utf<char16_t>(task.name);

			memmove(archiveHeader.archiveName,
				tmpU16str.c_str(),
				(std::min)(sizeof(ArchiveHeader::archiveName) - sizeof(char16_t),
					tmpU16str.length() * sizeof(char16_t)));
			stream.write(&archiveHeader, sizeof(ArchiveHeader));

			sectionHeader = SectionHeader{};

			logger->info("Generating File Data...");
			for (auto& tocTask : task.TOCs)
			{
				assert(folderList.size() <= std::numeric_limits<uint16_t>::max());
				assert(fileInfoList.size() <= std::numeric_limits<uint16_t>::max());
				TocEntry tocEntry{
					.firstFolderIndex = uint16_t(folderList.size()),
					.firstFileIndex = uint16_t(fileInfoList.size()),
					.startHierarchyFolderIndex = uint16_t(folderList.size()) };
				memmove(&tocEntry.name, tocTask.name.c_str(),
					(std::min)(sizeof(TocEntry::name) - 1, tocTask.name.length()));
				memmove(&tocEntry.alias, tocTask.alias.c_str(),
					(std::min)(sizeof(TocEntry::alias) - 1, tocTask.alias.length()));

				folderList.emplace_back();
				preBuildFolder(tocTask.rootFolder, tocEntry.firstFolderIndex);

				assert(folderList.size() <= std::numeric_limits<uint16_t>::max());
				assert(fileInfoList.size() <= std::numeric_limits<uint16_t>::max());
				tocEntry.lastFolderIndex = uint16_t(folderList.size());
				tocEntry.lastFileIndex = uint16_t(fileInfoList.size());
				tocList.push_back(tocEntry);
			}

			//concat _folderNameList and _fileNameList
			const auto baseOffset = uint32_t(folderNameList.back().offset +
				folderNameList.back().name.length() + 1);
			folderNameList.splice(folderNameList.end(), fileNameList);
			
			//and adjust files' name offset
			for (FileInfoEntry& entry : fileInfoList)
			{
				entry.fileNameOffset += baseOffset;
			}

			logger->info("Writing Section Header...");
			const size_t sectionHeaderBegPos = stream.getpos();
			stream.write(&sectionHeader, sizeof(SectionHeader));
			sectionHeader.TOC_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));
			
			assert(tocList.size() <= std::numeric_limits<uint16_t>::max());
			sectionHeader.TOC_count = uint16_t(tocList.size());
			
			for (TocEntry& entry : tocList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.Folder_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));

			assert(folderList.size() <= std::numeric_limits<uint16_t>::max());
			sectionHeader.Folder_count = uint16_t(folderList.size());
			
			for (FolderEntry& entry : folderList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.FileInfo_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));

			assert(fileInfoList.size() <= std::numeric_limits<uint16_t>::max());
			sectionHeader.FileInfo_count = uint16_t(fileInfoList.size());
			
			for (FileInfoEntry& entry : fileInfoList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.FileName_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));

			assert(folderNameList.size() <= std::numeric_limits<uint16_t>::max());
			sectionHeader.FileName_count = uint16_t(folderNameList.size());
			
			for (FileName& filename : folderNameList)
			{
				stream.write(filename.name.c_str(), filename.name.length() + 1);
			}

			archiveHeader.sectionHeaderSize =
				uint32_t(stream.getpos() - sectionHeaderBegPos);

			std::list<std::future<File>> l;
			logger->info("Starting File Compression..");
			for (size_t i = 0; i < task.TOCs.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.TOCs[i].rootFolder,
					folderList[tocList[i].startHierarchyFolderIndex],
					compress_level));
			}

			archiveHeader.exactFileDataOffset = uint32_t(stream.getpos());

			logger->info("Writing Compressed Files...");
			if (Archive::Write_Encrypted == mode)
			{
				stream.writeKey();
			}
			return std::async(std::launch::async,
				[this, skip_tool_signature, l = std::move(l)]() mutable
			{
				size_t fileswritten = 0;
				for (auto& filefuture : l)
				{
					auto file = filefuture.get();
					stream.write(file.getFileDataHeader(), sizeof(FileDataHeader));
					file.fileInfoEntry->fileDataOffset =
						uint32_t(stream.getpos() - archiveHeader.exactFileDataOffset);
					stream.write(file.compressedData.get_const(),
						file.fileInfoEntry->compressedLen);
					// calculate progress
					fileswritten++;
					logProgress(logger.get(), fileswritten, l.size(),
						file.getFileDataHeader()->fileName.ansi);
				}

				logger->info("Rewrite Section Header...");
				//rewrite sectionHeader
				stream.setpos(sizeof(ArchiveHeader));
				stream.write(&sectionHeader, sizeof(SectionHeader));

				if (Archive::Write_Encrypted == mode)
				{
					//rewrite TocEntry part
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.TOC_offset);
					for (TocEntry& entry : tocList)
					{
						stream.write(&entry, sizeof(entry));
					}
					//rewrite FolderEntry part 
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.Folder_offset);
					for (FolderEntry& entry : folderList)
					{
						stream.write(&entry, sizeof(entry));
					}
					//rewrite FileName part 
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.FileName_offset);
					for (FileName& filename : folderNameList)
					{
						stream.write(filename.name.c_str(), filename.name.length() + 1);
					}
				}

				//rewrite FileInfoEntry part
				stream.setpos(sizeof(ArchiveHeader) + sectionHeader.FileInfo_offset);
				for (FileInfoEntry& entry : fileInfoList)
				{
					stream.write(&entry, sizeof(entry));
				}

				//calculate archiveSignature
				logger->info("Calculating Archive Signature...");
				std::byte buffer[1024];
				std::byte md5_digest[16];
				MD5_CTX md5_context;
				MD5_Init(&md5_context);
				MD5_Update(&md5_context, ARCHIVE_SIG, sizeof(ARCHIVE_SIG) - 1);
				stream.setpos(sizeof(ArchiveHeader));
				size_t lengthToRead = archiveHeader.exactFileDataOffset - stream.getpos();
				while (lengthToRead > sizeof(buffer))
				{
					stream.read(buffer, sizeof(buffer));
					MD5_Update(&md5_context, buffer, sizeof(buffer));
					lengthToRead -= sizeof(buffer);
				}
				stream.read(buffer, lengthToRead);
				MD5_Update(&md5_context, buffer, lengthToRead);
				MD5_Final(reinterpret_cast<unsigned char*>(md5_digest), &md5_context);
				memmove(archiveHeader.archiveSignature, md5_digest, 16);

				if (skip_tool_signature)
				{
					logger->info("Tool Signature Calculation Skipped.");
				}
				else
				{
					//calculate toolSignature
					logger->info("Calculating Tool Signature...");
					MD5_Init(&md5_context);
					MD5_Update(&md5_context, TOOL_SIG, sizeof(TOOL_SIG) - 1);
					stream.setpos(sizeof(ArchiveHeader));
					size_t lenthRead = sizeof(buffer);
					while (lenthRead == sizeof(buffer))
					{
						lenthRead = stream.read(buffer, sizeof(buffer));
						MD5_Update(&md5_context, buffer, lenthRead);
					}
					MD5_Final(reinterpret_cast<unsigned char*>(md5_digest), &md5_context);
					memmove(archiveHeader.toolSignature, md5_digest, 16);
				}

				//rewrite archiveHeader
				logger->info("Rewrite Archive Header...");
				stream.setpos(0);
				stream.write(&archiveHeader, sizeof(ArchiveHeader));

				if (Archive::Write_Encrypted == mode)
				{
					stream.writeEncryptionEnd();
				}
				logger->info("=================");
				logger->info("Build Finished.");
			});
		}
	};
	Archive::Archive(const std::string& loggername)
	{
		_opened = false;
		_internal = std::unique_ptr<ArchiveInternal>(new ArchiveInternal);
		auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		_internal->logger =std::make_shared<spdlog::logger>(loggername, sink);
	}
	Archive::Archive(Archive&& o) noexcept
	{
		_opened = o._opened;
		o._opened = false;
		_internal = std::move(o._internal);
	}
	Archive& Archive::operator=(Archive&& o) noexcept
	{
		_opened = o._opened;
		o._opened = false;
		_internal = std::move(o._internal);
		return *this;
	}
	void Archive::open(
		const std::filesystem::path& filepath, Mode mode)
	{
		_internal->archiveHeader = ArchiveHeader{};
		_internal->sectionHeader = SectionHeader{};
		_internal->tocList.clear();
		_internal->folderList.clear();
		_internal->fileInfoList.clear();
		_internal->fileNameList.clear();
		_internal->folderNameList.clear();
		_internal->fileNameLookUpTable.clear();
		switch (mode)
		{
		case Read:
		{
			_internal->stream.open(filepath, stream::Read_EncryptionUnknown);
			_internal->stream.read(
				&_internal->archiveHeader, sizeof(ArchiveHeader));
			_internal->stream.read(
				&_internal->sectionHeader, sizeof(SectionHeader));

			_internal->stream.setpos(
				sizeof(ArchiveHeader) + _internal->sectionHeader.TOC_offset);
			for (uint16_t i = 0; i < _internal->sectionHeader.TOC_count; i++)
			{
				_internal->tocList.emplace_back();
				_internal->stream.read(
					&_internal->tocList.back(), sizeof(TocEntry));
			}

			_internal->stream.setpos(
				sizeof(ArchiveHeader) + _internal->sectionHeader.Folder_offset);
			for (uint16_t i = 0; i < _internal->sectionHeader.Folder_count; ++i)
			{
				_internal->folderList.emplace_back();
				_internal->stream.read(
					&_internal->folderList.back(), sizeof(FolderEntry));
			}

			_internal->stream.setpos(
				sizeof(ArchiveHeader) + _internal->sectionHeader.FileInfo_offset);
			for (uint16_t i = 0; i < _internal->sectionHeader.FileInfo_count; ++i)
			{
				_internal->fileInfoList.emplace_back();
				_internal->stream.read(
					&_internal->fileInfoList.back(), sizeof(FileInfoEntry));
			}

			const size_t filenameOffset =
				sizeof(ArchiveHeader) + _internal->sectionHeader.FileName_offset;
			_internal->stream.setpos(filenameOffset);
			for (uint16_t i = 0; i < _internal->sectionHeader.FileName_count; ++i)
			{
				_internal->fileNameList.emplace_back();
				_internal->fileNameList.back().offset =
					uint32_t(_internal->stream.getpos() - filenameOffset);
				_internal->fileNameList.back().name = u8"";
				char8_t c;
				while (_internal->stream.read(&c, 1), bool(c))
				{
					_internal->fileNameList.back().name += c;
				}
			}
			for (FileName& filename : _internal->fileNameList)
			{
				_internal->fileNameLookUpTable[filename.offset] = &filename;
			}
		}
		break;
		case Write_NonEncrypted:
		{
			_internal->stream.open(filepath, stream::Write_NonEncrypted);
		}
		break;
		default: // Write_Encrypted
		{
			_internal->stream.open(filepath, stream::Write_Encrypted);
		}
		break;
		}
		_internal->mode = mode;
		_opened = true;
	}
	std::future<void> Archive::extract(ThreadPool& pool, const std::filesystem::path& root)
	{
		assert(Read == _internal->mode);
		create_directories(root);
		std::list<std::tuple<std::future<File>, std::filesystem::path>> files;
		for (const TocEntry& toc : _internal->tocList)
		{
			auto r = _internal->extractFolder(pool, root / toc.name, toc.startHierarchyFolderIndex);
			files.splice(files.end(), r);
		}
		return std::async(std::launch::async,
			[this, files = std::move(files)]() mutable
		{
			size_t fileswritten = 0;
			for (auto& f : files)
			{
				const auto data = std::get<0>(f).get();
				const auto path = std::get<1>(f);
				create_directories(path.parent_path());
				{
					std::ofstream ofile(path, std::ios::binary);
					if (!ofile) throw FileIoError("Failed to open file: " +
						path.string() + " for output");
					ofile.write(
						reinterpret_cast<const char*>(data.decompressedData.get_const()),
						data.fileInfoEntry->decompressedLen);
				}
				/// \TODO waiting VS2019 to adapt the new c++20 clock_cast
				// last_write_time(path, std::chrono::clock_cast<file_clock>(system_clock::from_time_t(data.getFileDataHeader()->modificationDate)));

				// calculate progress
				fileswritten++;
				logProgress(_internal->logger.get(), fileswritten, files.size(),
					path.filename().string());
			}
		});
	}
	std::future<void> Archive::create(
		ThreadPool& pool,
		buildfile::Archive& task,
		const std::filesystem::path& root,
		int compress_level,
		bool skip_tool_signature,
		const std::vector<std::u8string>& ignore_list)
	{
		assert(Read != _internal->mode);
		return _internal->buildTask(
			_internal->parseTask(task, root, ignore_list),
			pool,
			compress_level,
			skip_tool_signature);
	}
	void Archive::listFiles() const
	{
		assert(Read == _internal->mode);
		for (TocEntry& toc : _internal->tocList)
		{
			std::cout << "TOCEntry\n";
			std::cout << "  Name : '" << toc.name << "'\n";
			std::cout << "  Alias: '" << toc.alias << "'\n\n";
			std::cout << std::left << std::setw(72) << "File Name"
				<< std::right << std::setw(16) << "Original Size"
				<< std::right << std::setw(16) << "Stored Size"
				<< std::right << std::setw(8) << "Ratio"
				<< std::right << std::setw(16) << "Storage Type\n";
			_internal->listFolder(toc.startHierarchyFolderIndex);
		}
		std::cout << '\n';
	}
	Json::Value Archive::getFileTree() const
	{
		assert(Read == _internal->mode);

		Json::Value r(Json::objectValue);
		r["name"] = boost::locale::conv::utf_to_utf<char>(
			std::u16string(_internal->archiveHeader.archiveName,
				sizeof(ArchiveHeader::archiveName) / sizeof(char16_t)));

		r["tocs"] = Json::Value(Json::arrayValue);
		for (TocEntry& toc : _internal->tocList)
		{
			Json::Value t(Json::objectValue);
			// use .c_str() to trim '\0' at end
			t["name"] = std::string(toc.name, sizeof(toc.name)).c_str();
			t["alias"] = std::string(toc.alias, sizeof(toc.alias)).c_str();
			t["tocfolder"] = _internal->getFolderTree(toc.startHierarchyFolderIndex);

			r["tocs"].append(t);
		}
		return r;
	}
	std::future<bool> Archive::testArchive(ThreadPool& pool)
	{
		assert(Read == _internal->mode);
		std::list<std::tuple<std::future<File>, std::filesystem::path>> files;
		for (const TocEntry& toc : _internal->tocList)
		{
			auto r = _internal->extractFolder(pool, "", toc.startHierarchyFolderIndex);
			files.splice(files.end(), r);
		}
		return std::async(std::launch::async,
			[files = std::move(files)]() mutable
		{
			for (auto& f : files)
			{
				const auto data = std::get<0>(f).get();
				if (data.getFileDataHeader()->CRC !=
					crc32(crc32(0, nullptr, 0),
						reinterpret_cast<const Bytef*>(data.decompressedData.get_const()),
						data.fileInfoEntry->decompressedLen))
				{
					return false;
				}	
			}
			return true;
		});
	}
	std::u8string Archive::getArchiveSignature() const
	{
		return reinterpret_cast<char8_t*>(_internal->archiveHeader.archiveSignature);
	}
	void Archive::close()
	{
		if(_opened)	_internal->stream.close();
	}
	Archive::~Archive()
	{
		close();
	}
}
