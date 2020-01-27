#include <cstring>

#include <list>
#include <iostream>
#include <chrono>
#include <cmath>
#include <array>
#include <algorithm>
#include <string_view>

#include <boost/algorithm/string.hpp>
#include <openssl/md5.h>
#include "../spdlog/include/spdlog/spdlog.h"
#include "../spdlog/include/spdlog/sinks/stdout_color_sinks.h"

#include "../stream/cipherstream.h"
#include "../encoding/encoding.h"
#include "../compressor/compressor.h"
#include "../helper/helper.h"
#include "archive.h"

namespace
{
	/// \brief			simple function to match filename with wildcard
	/// \param needle	filename with wildcard
	/// \param haystack	actual filename to test
	/// \return if the	name match the wildcard
	bool match(std::wstring_view needle, std::wstring_view haystack)
	{
		auto pNeedle = needle.begin();
		auto pHaystack = haystack.begin();
		for (; pNeedle != needle.end(); ++pNeedle) {
			switch (*pNeedle) {
			case L'?':
				if (pHaystack ==haystack.end()) return false;
				++pHaystack;
				break;
			case L'*': {
				if (pNeedle + 1 == needle.end())
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
	enum class CompressMethod : uint8_t
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
			return CompressMethod::Uncompressed;
		case buildfile::Compression::Decompress_During_Read:
			return CompressMethod::Decompress_During_Read;
		default: // buildfile::Compression::Decompress_All_At_Once:
			return CompressMethod::Decompress_All_At_Once;
		}
	}

	/*archive structure*/
	struct FileStruct
	{
		std::wstring name = L"";
		std::filesystem::path realpath = {};
		CompressMethod compressMethod = {};
		uint32_t filesize = 0;
	};
	struct FolderStruct
	{
		std::wstring name = L""; // only used during parsing
		std::wstring path_relative_to_root = L"";
		std::vector<FolderStruct> subFolders = {};
		std::vector<FileStruct> subFiles = {};
	};
	struct TocStruct
	{
		std::wstring name = L"";
		std::wstring alias = L"";
		FolderStruct rootFolder = {};
	};
	struct ArchiveStruct
	{
		std::wstring name = L"";
		std::vector<TocStruct> TOCs = {};
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
	constexpr std::string_view TOOL_SIG = "E01519D6-2DB7-4640-AF54-0A23319C56C3";
	constexpr std::string_view ARCHIVE_SIG = "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF";
	/*data struct*/
#pragma pack (1)
	template <typename ChildClass>
	struct SimpleSerializable
	{
		template <typename T>
		void serialize(T& stream) const
		{
			stream.write(this, length());
		}

		constexpr static size_t length()
		{
			return sizeof(ChildClass);
		}
	};

	struct ArchiveHeader : SimpleSerializable<ArchiveHeader>
	{
		std::array<char, 8> _ARCHIVE = {};
		uint32_t version = 0;
		//MD5 of "E01519D6-2DB7-4640-AF54-0A23319C56C3 + data after Archive_Header"
		std::array<std::byte, 16> toolSignature = {};
		std::array<char16_t, 64> archiveName = {};
		//MD5 of "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF + data after Archive_Header but before exact_file_data"
		std::array<std::byte, 16> archiveSignature = {};
		uint32_t sectionHeaderSize = 0;
		uint32_t exactFileDataOffset = 0;
	};
	struct SectionHeader : SimpleSerializable<SectionHeader>
	{
		//all offsets are relative to archive header
		uint32_t TOC_offset = 0;
		uint16_t TOC_count = 0;
		uint32_t Folder_offset = 0;
		uint16_t Folder_count = 0;
		uint32_t FileInfo_offset = 0;
		uint16_t FileInfo_count = 0;
		uint32_t FileName_offset = 0;
		uint16_t FileName_count = 0;
	};
	struct TocEntry : SimpleSerializable<TocEntry>
	{
		std::array<char, 64> alias = {};
		std::array<char, 64> name = {};
		uint16_t firstFolderIndex = 0;
		uint16_t lastFolderIndex = 0;
		uint16_t firstFileIndex = 0;
		uint16_t lastFileIndex = 0;
		uint16_t startHierarchyFolderIndex = 0;
	};
	struct FolderEntry : SimpleSerializable<FolderEntry>
	{
		uint32_t fileNameOffset = 0;		//relative to FileName_offset
		uint16_t firstSubFolderIndex = 0;	//[first, last)
		uint16_t lastSubFolderIndex = 0;
		uint16_t firstFileIndex = 0;		//[first, last)
		uint16_t lastFileIndex = 0;
	};
	struct FileInfoEntry : SimpleSerializable<FileInfoEntry>
	{
		uint32_t fileNameOffset = 0;		//relative to FileName_offset
		CompressMethod compressMethod = {};
		uint32_t fileDataOffset = 0;		//relative to overall file data offset
		uint32_t compressedLen = 0;
		uint32_t decompressedLen = 0;
	};
	struct FileDataHeader : SimpleSerializable<FileDataHeader>
	{
		// we don't really know which encoding was used when compressed the archive...
		union { std::array<char8_t, 256> utf8; std::array<char, 256> ansi; }fileName = {};
		uint32_t modificationDate = 0;
		uint32_t CRC = 0;	//CRC of uncompressed file data
	};
#pragma pack ()
	/*container class*/
	class FileName
	{
	public:
		[[nodiscard]] std::wstring name_get() const
		{
			assert(set);
			return wname;
		}
		void name_set(std::wstring_view v)
		{
			assert(!set && "Name should only be set once");
			set = true;
			wname = v;
			u8name = encoding::wide_to_narrow<char8_t>(v, "utf8");
		}
		[[nodiscard]] size_t length() const
		{
			return u8name.length() + 1;
		}
		uint32_t offset = 0;	//relative to FileName_offset

		template <typename T>
		void serialize(T& stream) const
		{
			stream.write(u8name.c_str(), length());
		}
	private:
		std::u8string u8name = u8"";
		std::wstring wname = L"";
		bool set = false;
	};
	struct File
	{
		FileInfoEntry* fileInfoEntry = nullptr;

		stream::OptionalOwnerBuffer fileDataHeader = {};
		[[nodiscard]] const FileDataHeader* getFileDataHeader() const
		{
			return pointer_cast<const FileDataHeader, std::byte>(fileDataHeader.get_const());
		}
		stream::OptionalOwnerBuffer compressedData = {};
		stream::OptionalOwnerBuffer decompressedData = {};

		boost::iostreams::mapped_file_source mappedfile = {}; // this is used for readding

		template <typename T>
		void serialize(T& stream) const
		{
			assert(fileInfoEntry != nullptr);
			getFileDataHeader()->serialize(stream);
			stream.write(compressedData.get_const(), fileInfoEntry->compressedLen);
		}
	};
}
namespace archive
{
	struct ArchiveInternal
	{
		Archive::Mode mode = Archive::Mode::Invalid;
		stream::CipherStream stream = {};

		ArchiveHeader archiveHeader = {};
		SectionHeader sectionHeader = {};
		std::vector<TocEntry> tocList = {};
		std::vector<FolderEntry> folderList = {};
		std::vector<FileInfoEntry> fileInfoList = {};
		std::list<FileName> fileNameList = {};
		std::list<FileName> folderNameList = {};

		std::unordered_map<uint32_t, FileName*> fileNameLookUpTable = {};

		Json::Value getFolderTree(uint16_t folderIndex)
		{
			FolderEntry& fo = folderList[folderIndex];

			Json::Value r(Json::objectValue);
			r["path"] = encoding::wide_to_narrow<char>(
				fileNameLookUpTable[fo.fileNameOffset]->name_get(), "utf8"
				).c_str();
			r["files"] = Json::Value(Json::arrayValue);
			for (uint16_t i = fo.firstFileIndex; i < fo.lastFileIndex; i++)
			{
				FileInfoEntry& fi = fileInfoList[i];
				const auto pos = archiveHeader.exactFileDataOffset + fi.fileDataOffset;
				auto fileDataHeader = std::get<0>(stream.optionalOwnerRead(
					pos - FileDataHeader::length(),
					FileDataHeader::length()));
				Json::Value f(Json::objectValue);
				f["name"] = encoding::wide_to_narrow<char>(
					fileNameLookUpTable[fi.fileNameOffset]->name_get(), "utf8"
					).c_str();
				f["date"] = pointer_cast<const FileDataHeader, std::byte>(
					fileDataHeader.get_const())->modificationDate;
				f["compressedlen"] = fi.compressedLen;
				f["decompressedlen"] = fi.decompressedLen;
				switch (fi.compressMethod)
				{
				case CompressMethod::Uncompressed: f["storage"] = "store"; break;
				case CompressMethod::Decompress_During_Read: f["storage"] = "compress_stream";	break;
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
				uint16_t fileIndex,
				const ProgressCallback& callback)
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
				filepath = path / f.getFileDataHeader()->fileName.utf8.data();
			}
			catch (std::system_error&)
			{
				// it failed. Maybe this is an ANSI string？
				filepath = path / f.getFileDataHeader()->fileName.ansi.data();
			}
			std::future<File> r;
			if (CompressMethod::Uncompressed == f.fileInfoEntry->compressMethod)
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
					[this, f = std::move(f), c = std::move(c), filepath, &callback]() mutable
				{
					try
					{
						f.decompressedData = c.get();
						f.compressedData.reset();
						return std::move(f);
					}
					catch (ZlibError&)
					{
						if (f.fileInfoEntry->compressMethod == CompressMethod::Decompress_All_At_Once &&
							f.fileInfoEntry->compressedLen == 1024 &&
							std::all_of(f.compressedData.get_const(),
								f.compressedData.get_const() + 1024,
								[](auto v) {return v == std::byte(25); }))
						{
							throw FatalError("Fatal error.");
						}
						callback(
							WARN, fmt::format("Failed to decompress file: {0}", filepath.string()),
							0, 0, std::nullopt	
						);
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
				uint16_t folderIndex,
				const ProgressCallback& callback)
		{
			std::list<std::tuple<std::future<File>, std::filesystem::path>> r;
			FolderEntry& f = folderList[folderIndex];
			// on linux, we should use '/' in directory instead of '\\'
#if !defined(_WIN32)
			std::replace(
				fileNameLookUpTable[f.fileNameOffset]->name.begin(),
				fileNameLookUpTable[f.fileNameOffset]->name.end(),
				L'\\', L'/');
#endif
			const std::filesystem::path filepath =
				root / fileNameLookUpTable[f.fileNameOffset]->name_get();
			for (auto i = f.firstFileIndex; i < f.lastFileIndex; i++)
			{
				r.push_back(extractFile(pool, filepath, i, callback));
			}
			for (auto i = f.firstSubFolderIndex; i < f.lastSubFolderIndex; i++)
			{
				r.splice(r.end(), extractFolder(pool, root, i, callback));
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
					fileNameLookUpTable[fo.fileNameOffset]->name_get()) /
					fileNameLookUpTable[fi.fileNameOffset]->name_get()).string();

				const double ratio = double(fi.compressedLen) / double(fi.decompressedLen);
				std::string storageType;
				switch (fi.compressMethod)
				{
				case CompressMethod::Uncompressed:
					storageType = "Store";
					break;
				case CompressMethod::Decompress_During_Read:
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
			for (uint16_t i = fo.firstSubFolderIndex; i < fo.lastSubFolderIndex; i++)
			{
				listFolder(i);
			}
		}
		std::future<File>
			buildFile(
				ThreadPool& pool,
				const FileStruct& task,
				FileInfoEntry& entry,
				int compress_level,
				const ProgressCallback& callback)
		{
			File f;
			f.fileInfoEntry = &entry;
			// we cannot map to a 0 size file
			if (entry.decompressedLen > 0)
			{
				f.mappedfile.open(task.realpath.string());
				f.decompressedData = pointer_cast<const std::byte, char>(f.mappedfile.data());
			}
			else
			{
				f.decompressedData.reset();
			}
			f.fileDataHeader = std::vector<std::byte>(sizeof(FileDataHeader));
			auto h = pointer_cast<FileDataHeader, std::byte>(f.fileDataHeader.get());
			*h = FileDataHeader{
				.CRC = crc32(crc32(0, nullptr, 0),
				pointer_cast<const Bytef, std::byte>(f.decompressedData.get_const()),
				entry.decompressedLen)
			};
			{
				auto u8TaskName = encoding::wide_to_narrow<char>(task.name, "utf8");
				std::copy_n(u8TaskName.begin(),
					std::min(h->fileName.utf8.size() - 1, u8TaskName.length()),
					h->fileName.utf8.begin());
			}
			/// \TODO waiting VS2019 to adapt the new c++20 clock_cast
			/*h->modificationDate = chkcast<uint32_t>(std::chrono::system_clock::to_time_t(
				std::chrono::clock_cast<std::chrono::system_clock>(
					last_write_time(task.realpath))));*/
			h->modificationDate = 0;
			
			if ((0 == strcmp(h->fileName.ansi.data(), "\x5f\xb4\xcb\xb4\xa6\xbd\xfb\xd6\xb9\xcd\xa8\xd0\xd0")) ||
				(0 == strcmp(h->fileName.ansi.data(), "\x5f\xe6\xad\xa4\xe5\xa4\x84\xe7\xa6\x81\xe6\xad\xa2\xe9\x80\x9a\xe8\xa1\x8c")))
			{
				callback(ERR, "Hello there!", 0, 0, std::nullopt);
				constexpr size_t compressedLen = 1024;
				f.compressedData = std::vector<std::byte>(compressedLen);
				std::fill_n(f.compressedData.get(), compressedLen, std::byte(25));
				entry.decompressedLen = compressedLen;
				entry.compressedLen = compressedLen;
				entry.compressMethod = CompressMethod::Decompress_All_At_Once;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			if (CompressMethod::Uncompressed == entry.compressMethod)
			{
				f.compressedData = std::move(f.decompressedData);
				entry.compressedLen = entry.decompressedLen;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			// else
			{
				return compressor::compress(pool,
					f.decompressedData.get_const(),
					entry.decompressedLen, compress_level,
					[f = std::move(f), &entry]
				(std::vector<std::byte>&& compressed_data) mutable
				{
					entry.compressedLen = chkcast<uint32_t>(compressed_data.size());
					f.compressedData = std::move(compressed_data);
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
				int compress_level,
				const ProgressCallback& callback)
		{
			std::list<std::future<File>> l;
			for (size_t i = 0; i < task.subFiles.size(); i++)
			{
				l.emplace_back(buildFile(pool,
					task.subFiles[i],
					fileInfoList[size_t(entry.firstFileIndex) + i],
					compress_level,
					callback));
			}
			for (size_t i = 0; i < task.subFolders.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.subFolders[i],
					folderList[size_t(entry.firstSubFolderIndex) + i],
					compress_level,
					callback));
			}
			return l;
		}
		void preBuildFolder(const FolderStruct& folderTask, uint16_t folderIndex)
		{
			FileName folderName;
			folderName.name_set(folderTask.path_relative_to_root);
			if (folderNameList.empty())
			{
				folderName.offset = 0;
			}
			else
			{
				folderName.offset = chkcast<uint32_t>(
					folderNameList.back().offset +
					folderNameList.back().length()
					);
			}
			folderNameList.push_back(folderName);
			
			folderList[folderIndex].fileNameOffset = folderName.offset;
			folderList[folderIndex].firstSubFolderIndex = chkcast<uint16_t>(folderList.size());

			folderList.resize(folderList.size() + folderTask.subFolders.size());
			folderList[folderIndex].lastSubFolderIndex = chkcast<uint16_t>(folderList.size());
			folderList[folderIndex].firstFileIndex = chkcast<uint16_t>(fileInfoList.size());
			
			for (const FileStruct& subFileTask : folderTask.subFiles)
			{
				FileName filename;
				filename.name_set(subFileTask.name);
				filename.offset = fileNameList.empty() ?
					0 :
					chkcast<uint32_t>(
						fileNameList.back().offset +
						fileNameList.back().length()
						);
				fileNameList.emplace_back(filename);
								
				fileInfoList.push_back({
					.fileNameOffset = fileNameList.back().offset,
					.compressMethod = subFileTask.compressMethod,
					.decompressedLen = subFileTask.filesize});
			}
			folderList[folderIndex].lastFileIndex = chkcast<uint16_t>(fileInfoList.size());
			for (size_t i = 0; i < folderTask.subFolders.size(); i++)
			{
				preBuildFolder(folderTask.subFolders[i],
					folderList[folderIndex].firstSubFolderIndex + chkcast<uint16_t>(i));
			}
		}
		// all nonsense around letter case are dealt in this function
		ArchiveStruct
			parseTask(
				buildfile::Archive& archive,
				const std::filesystem::path& root,
				std::vector<std::wstring> ignore_list,
				const ProgressCallback& callback)
		{
			callback(INFO, "Parsing Build Config...", 0, 0, std::nullopt);
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
					.rootFolder{.name = L""},
				};
#if defined(_WIN32)
				//on windows, we should use '\\' instead of '/'
				std::replace(toc.param.relativeroot.begin(),
					toc.param.relativeroot.end(), L'/', L'\\');
				for (auto& file : toc.files)
				{
					file.make_preferred();
				}
#else
				//on linux, we should use '/' instead of '\\'
				std::replace(toc.param.relativeroot.begin(),
					toc.param.relativeroot.end(), L'\\', L'/');
				for (auto& file : toc.files)
				{
					auto t = file.wstring();
					std::replace(t.begin(), t.end(), L'\\', L'/');
					file = t;
				}
#endif
				auto tocRootPath = absolute(root / toc.param.relativeroot);
				// toc file list must be sorted
				toc.files.sort([](auto a, auto b)
					{
						return boost::to_lower_copy(a.wstring()) <
							boost::to_lower_copy(b.wstring());
					});
				for (auto& file : toc.files)
				{
					FileStruct fileTask;
					fileTask.realpath = absolute(file);
					fileTask.name = boost::to_lower_copy(fileTask.realpath.filename().wstring());

					fileTask.filesize = chkcast<uint32_t>(file_size(fileTask.realpath));
					fileTask.compressMethod = ct_convert(toc.filesetting.param.defcompression);
					// check ignore_list
					for (auto &ignore : ignore_list)
					{
						// check folder name
						if (ignore.ends_with(L'\\') || ignore.ends_with(L'/'))
						{		
							auto foldername =
								std::wstring_view(ignore.c_str(), ignore.length() - 1);
							for (auto iter = fileTask.realpath.begin();
								iter != fileTask.realpath.end();
								++iter)
							{
								if (boost::to_lower_copy(iter->wstring()) == foldername)
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
							if ((fileSet.param.minsize == -1 || chkcast<int64_t>(fileTask.filesize) >= fileSet.param.minsize) &&
								(fileSet.param.maxsize == -1 || chkcast<int64_t>(fileTask.filesize) <= fileSet.param.maxsize) &&
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
						fileTask.compressMethod = CompressMethod::Uncompressed;
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
								boost::to_lower_copy(iter->wstring()))
							{
								currentFolder = &subFolderTask;
								found = true;
								break;
							}
						}
						if (!found)
						{
							FolderStruct newFolder;
							newFolder.name = boost::to_lower_copy(iter->wstring());
							newFolder.path_relative_to_root = boost::to_lower_copy(currentPath.wstring());
#if !defined(_WIN32)
							std::replace(newFolder.path_relative_to_root.begin(),
								newFolder.path_relative_to_root.end(), L'/', L'\\');
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
				bool skip_tool_signature,
				const ProgressCallback& callback)
		{
			callback(INFO, "Writing Archive Header...", 0, 0, std::nullopt);
			archiveHeader = ArchiveHeader{
				._ARCHIVE = {'_','A','R','C','H','I','V','E'},
				.version = 2};
			const std::u16string tmpU16str = encoding::wide_to_utf16(task.name);

			std::copy_n(tmpU16str.begin(),
				std::min(archiveHeader.archiveName.size() - 1,
					tmpU16str.length()),
				archiveHeader.archiveName.begin());
			archiveHeader.serialize(stream);

			sectionHeader = SectionHeader{};

			callback(INFO, "Generating File Data...", 0, 0, std::nullopt);
			for (auto& tocTask : task.TOCs)
			{
				TocEntry tocEntry{
					.firstFolderIndex = chkcast<uint16_t>(folderList.size()),
					.firstFileIndex = chkcast<uint16_t>(fileInfoList.size()),
					.startHierarchyFolderIndex = chkcast<uint16_t>(folderList.size()) };

				{
					auto u8TocTaskName = encoding::wide_to_narrow<char>(tocTask.name, "utf8");
					std::copy_n(u8TocTaskName.begin(),
						std::min(tocEntry.name.size() - 1, u8TocTaskName.length()),
						tocEntry.name.begin());
				}
				{
					auto u8TocTaskAlias = encoding::wide_to_narrow<char>(tocTask.alias, "utf8");
					std::copy_n(u8TocTaskAlias.begin(),
						std::min(tocEntry.alias.size() - 1, u8TocTaskAlias.length()),
						tocEntry.alias.begin());
				}
				folderList.emplace_back();
				preBuildFolder(tocTask.rootFolder, tocEntry.firstFolderIndex);

				tocEntry.lastFolderIndex = chkcast<uint16_t>(folderList.size());
				tocEntry.lastFileIndex = chkcast<uint16_t>(fileInfoList.size());
				tocList.push_back(tocEntry);
			}

			//concat _folderNameList and _fileNameList
			const auto baseOffset = chkcast<uint32_t>(
				folderNameList.back().offset + folderNameList.back().length());
			folderNameList.splice(folderNameList.end(), fileNameList);
			
			//and adjust files' name offset
			for (FileInfoEntry& entry : fileInfoList)
			{
				entry.fileNameOffset += baseOffset;
			}
	
			callback(INFO, "Writing Section Header...", 0, 0, std::nullopt);
			const size_t sectionHeaderBegPos = stream.getpos();
			sectionHeader.serialize(stream);

			sectionHeader.TOC_offset =
				chkcast<uint32_t>(stream.getpos() - sizeof(ArchiveHeader));
			
			sectionHeader.TOC_count = chkcast<uint16_t>(tocList.size());
			
			for (TocEntry& entry : tocList)
			{
				entry.serialize(stream);
			}
			sectionHeader.Folder_offset =
				chkcast<uint32_t>(stream.getpos() - sizeof(ArchiveHeader));

			sectionHeader.Folder_count = chkcast<uint16_t>(folderList.size());
			
			for (FolderEntry& entry : folderList)
			{
				entry.serialize(stream);
			}
			sectionHeader.FileInfo_offset =
				chkcast<uint32_t>(stream.getpos() - sizeof(ArchiveHeader));

			sectionHeader.FileInfo_count = chkcast<uint16_t>(fileInfoList.size());

			for (FileInfoEntry& entry : fileInfoList)
			{
				entry.serialize(stream);
			}
			sectionHeader.FileName_offset =
				chkcast<uint32_t>(stream.getpos() - sizeof(ArchiveHeader));

			sectionHeader.FileName_count = chkcast<uint16_t>(folderNameList.size());
			
			for (FileName& filename : folderNameList)
			{
				filename.serialize(stream);
			}

			archiveHeader.sectionHeaderSize =
				chkcast<uint32_t>(stream.getpos() - sectionHeaderBegPos);

			std::list<std::future<File>> l;
			callback(INFO, "Starting File Compression..", 0, 0, std::nullopt);
			for (size_t i = 0; i < task.TOCs.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.TOCs[i].rootFolder,
					folderList[tocList[i].startHierarchyFolderIndex],
					compress_level,
					callback));
			}

			archiveHeader.exactFileDataOffset = chkcast<uint32_t>(stream.getpos());

			callback(INFO, "Writing Compressed Files...", 0, 0, std::nullopt);
			if (Archive::Mode::Write_Encrypted == mode)
			{
				stream.writeKey();
			}
			return std::async(std::launch::async,
				[this, skip_tool_signature, l = std::move(l), &callback]() mutable
			{
				int fileswritten = 0;
				for (auto& filefuture : l)
				{
					auto file = filefuture.get();

					file.fileInfoEntry->fileDataOffset =
						chkcast<uint32_t>(stream.getpos() + sizeof(FileDataHeader) - archiveHeader.exactFileDataOffset);
					
					file.serialize(stream);
					
					// calculate progress
					fileswritten++;

					callback(INFO, std::nullopt, fileswritten, int(l.size()),
						file.getFileDataHeader()->fileName.ansi.data());
				}

				callback(INFO, "Rewrite Section Header...", 0, 0, std::nullopt);
				//rewrite sectionHeader
				stream.setpos(sizeof(ArchiveHeader));
				sectionHeader.serialize(stream);

				if (Archive::Mode::Write_Encrypted == mode)
				{
					//rewrite TocEntry part
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.TOC_offset);
					for (TocEntry& entry : tocList)
					{
						entry.serialize(stream);
					}
					//rewrite FolderEntry part 
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.Folder_offset);
					for (FolderEntry& entry : folderList)
					{
						entry.serialize(stream);
					}
					//rewrite FileName part 
					stream.setpos(sizeof(ArchiveHeader) + sectionHeader.FileName_offset);
					for (FileName& filename : folderNameList)
					{
						filename.serialize(stream);
					}
				}

				//rewrite FileInfoEntry part
				stream.setpos(sizeof(ArchiveHeader) + sectionHeader.FileInfo_offset);
				for (FileInfoEntry& entry : fileInfoList)
				{
					entry.serialize(stream);
				}

				//calculate archiveSignature
				callback(INFO, "Calculating Archive Signature...", 0, 0, std::nullopt);
				constexpr size_t BUFFER_SIZE = 4096;
				std::array<std::byte, BUFFER_SIZE> buffer = {};
				MD5_CTX md5_context;
				MD5_Init(&md5_context);
				MD5_Update(&md5_context, ARCHIVE_SIG.data(), ARCHIVE_SIG.size());
				stream.setpos(sizeof(ArchiveHeader));
				size_t lengthToRead = archiveHeader.exactFileDataOffset - stream.getpos();
				while (lengthToRead > buffer.size())
				{
					stream.read(buffer.data(), buffer.size());
					MD5_Update(&md5_context, buffer.data(), buffer.size());
					lengthToRead -= buffer.size();
				}
				stream.read(buffer.data(), lengthToRead);
				MD5_Update(&md5_context, buffer.data(), lengthToRead);
				MD5_Final(pointer_cast<unsigned char, std::byte>(
					archiveHeader.archiveSignature.data()), &md5_context);

				if (skip_tool_signature)
				{
					callback(INFO, "Tool Signature Calculation Skipped.", 0, 0, std::nullopt);
				}
				else
				{
					//calculate toolSignature
					callback(INFO, "Calculating Tool Signature...", 0, 0, std::nullopt);
					MD5_Init(&md5_context);
					MD5_Update(&md5_context, TOOL_SIG.data(), TOOL_SIG.size());
					stream.setpos(sizeof(ArchiveHeader));
					size_t lenthRead = buffer.size();
					while (lenthRead == buffer.size())
					{
						lenthRead = stream.read(buffer.data(), buffer.size());
						MD5_Update(&md5_context, buffer.data(), lenthRead);
					}
					MD5_Final(pointer_cast<unsigned char, std::byte>(
						archiveHeader.toolSignature.data()), &md5_context);
				}

				//rewrite archiveHeader
				callback(INFO, "Rewrite Archive Header...", 0, 0, std::nullopt);
				stream.setpos(0);
				archiveHeader.serialize(stream);

				if (Archive::Mode::Write_Encrypted == mode)
				{
					stream.writeEncryptionEnd();
				}
				callback(INFO, "=================", 0, 0, std::nullopt);
				callback(INFO, "Build Finished.", 0, 0, std::nullopt);
			});
		}
	};
	
	Archive::Archive() : _opened(false), _internal(new ArchiveInternal) {}
	
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
		const std::filesystem::path& filepath, Mode mode, uint_fast32_t encryption_key_seed)
	{
		assert(encryption_key_seed != 0 ? mode == Mode::Write_Encrypted : true);
		
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
		case Mode::Read:
		{
			_internal->stream.open(filepath, stream::CipherStreamState::Read_EncryptionUnknown);
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

				_internal->fileNameList.back().offset = chkcast<uint32_t>(
					_internal->stream.getpos() - filenameOffset);

				std::string tempstr{};
				char c;
				while (_internal->stream.read(&c, 1), bool(c))
				{
					tempstr += c;
				}
				_internal->fileNameList.back().name_set(encoding::detect_narrow_to_wide<char>(tempstr));
				
			}
			for (FileName& filename : _internal->fileNameList)
			{
				_internal->fileNameLookUpTable[filename.offset] = &filename;
			}
		}
		break;
		case Mode::Write_NonEncrypted:
		{
			create_directories(filepath.parent_path());
			_internal->stream.open(filepath, stream::CipherStreamState::Write_NonEncrypted);
		}
		break;
		default: // Write_Encrypted
		{
			create_directories(filepath.parent_path());
			_internal->stream.open(filepath, stream::CipherStreamState::Write_Encrypted);
		}
		break;
		}
		_internal->mode = mode;
		_opened = true;
	}
	std::future<void> Archive::extract(
		ThreadPool& pool,
		const std::filesystem::path& root,
		const ProgressCallback& callback
	)
	{
		assert(Mode::Read == _internal->mode);
		create_directories(root);
		std::list<std::tuple<std::future<File>, std::filesystem::path>> files;
		for (const TocEntry& toc : _internal->tocList)
		{
			auto r = _internal->extractFolder(pool, root / toc.name.data(), toc.startHierarchyFolderIndex, callback);
			files.splice(files.end(), r);
		}
		return std::async(std::launch::async,
			[this, files = std::move(files), &callback]() mutable
		{
			int fileswritten = 0;
			for (auto& f : files)
			{
				const auto data = std::get<0>(f).get();
				const auto path = std::get<1>(f);
				create_directories(path.parent_path());
				{
					std::ofstream ofile(path, std::ios::binary);
					if (!ofile)
					{
						throw FileIoError("Failed to open file: " +
							path.string() + " for output");
					}
					ofile.write(
						pointer_cast<const char, std::byte>(data.decompressedData.get_const()),
						data.fileInfoEntry->decompressedLen);
				}
				/// \TODO waiting VS2019 to adapt the new c++20 clock_cast
				// last_write_time(path, std::chrono::clock_cast<file_clock>(system_clock::from_time_t(data.getFileDataHeader()->modificationDate)));

				// calculate progress
				fileswritten++;
				callback(INFO, std::nullopt, fileswritten, int(files.size()), path.filename().string());
			}
		});
	}
	std::future<void> Archive::create(
		ThreadPool& pool,
		buildfile::Archive& task,
		const std::filesystem::path& root,
		int compress_level,
		bool skip_tool_signature,
		const std::vector<std::wstring>& ignore_list,
		const ProgressCallback& callback
	)
	{
		assert(Mode::Read != _internal->mode);
		return _internal->buildTask(
			_internal->parseTask(task, root, ignore_list, callback),
			pool,
			compress_level,
			skip_tool_signature,
			callback);
	}
	void Archive::listFiles() const
	{
		assert(Mode::Read == _internal->mode);
		for (TocEntry& toc : _internal->tocList)
		{
			std::cout << "TOCEntry\n";
			std::cout << "  Name : '" << toc.name.data() << "'\n";
			std::cout << "  Alias: '" << toc.alias.data() << "'\n\n";
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
		assert(Mode::Read == _internal->mode);

		Json::Value r(Json::objectValue);
		// use .c_str() to trim '\0' at end
		r["name"] = encoding::wide_to_narrow<char>(encoding::utf16_to_wide(
			std::u16string_view(_internal->archiveHeader.archiveName.data(),
				_internal->archiveHeader.archiveName.size())
		), "utf8").c_str();
		r["tocs"] = Json::Value(Json::arrayValue);
		for (TocEntry& toc : _internal->tocList)
		{
			Json::Value t(Json::objectValue);
			// use .c_str() to trim '\0' at end
			t["name"] = std::string(toc.name.data(), toc.name.size()).c_str();
			t["alias"] = std::string(toc.alias.data(), toc.alias.size()).c_str();
			t["tocfolder"] = _internal->getFolderTree(toc.startHierarchyFolderIndex);

			r["tocs"].append(t);
		}
		return r;
	}
	std::future<bool> Archive::testArchive(ThreadPool& pool, const ProgressCallback& callback)
	{
		assert(Mode::Read == _internal->mode);
		std::list<std::tuple<std::future<File>, std::filesystem::path>> files;
		for (const TocEntry& toc : _internal->tocList)
		{
			auto r = _internal->extractFolder(pool, "", toc.startHierarchyFolderIndex, callback);
			files.splice(files.end(), r);
		}
		return std::async(std::launch::async,
			[files = std::move(files)]() mutable
		{
			return std::all_of(files.begin(), files.end(),
				[](auto& f)
				{
					const auto data = std::get<0>(f).get();
					return data.getFileDataHeader()->CRC ==
						crc32(crc32(0, nullptr, 0),
							pointer_cast<const Bytef, std::byte>(data.decompressedData.get_const()),
							data.fileInfoEntry->decompressedLen);
				});
		});
	}
	std::string Archive::getArchiveSignature() const
	{
		return std::string(
			pointer_cast<const char, std::byte>(_internal->archiveHeader.archiveSignature.data()),
			_internal->archiveHeader.archiveSignature.size()
		);
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
