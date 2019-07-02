#include <list>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>
#include <openssl/md5.h>

#include "../stream/cipherstream.h"

#include "../compressor/compressor.h"
#include "archive.h"

namespace
{
	/// \brief simple	function to match filename with wildcard
	/// \param needle	filename with wildcard
	/// \param haystack	actual filename to test
	/// \return if the	name match the wildcard
	static bool match(std::u8string_view needle, std::u8string_view haystack)
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
				if (*pHaystack != *pNeedle)	return false;
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
			return CompressMethod::Uncompressed;
		case buildfile::Compression::Decompress_During_Read:
			return CompressMethod::Decompress_During_Read;
		case buildfile::Compression::Decompress_All_At_Once:
			return CompressMethod::Decompress_All_At_Once;
		default:
			assert(false);
			return CompressMethod(0);
		}
	}

	/*task struct*/
	struct BuildFileTask
	{
		std::u8string name;
		std::filesystem::path realpath;
		CompressMethod compressMethod;
		uint32_t filesize;
	};
	struct BuildFolderTask
	{
		std::u8string name; // only used during parsing
		std::u8string path_relative_to_root;
		std::vector<BuildFolderTask> subFolderTasks;
		std::vector<BuildFileTask> subFileTasks;
	};
	struct BuildTOCTask
	{
		std::u8string name;
		std::u8string alias;
		BuildFolderTask rootFolderTask;
	};
	struct BuildArchiveTask
	{
		std::u8string name;
		std::vector<BuildTOCTask> buildTOCTasks;
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
	const char TOOL_SIG[] = "E01519D6-2DB7-4640-AF54-0A23319C56C3";
	const char ARCHIVE_SIG[] = "DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF";
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
		char fileName[256];
		uint32_t modificationDate;
		uint32_t CRC;	//CRC of uncompressed file data
	};
#pragma pack ()
	/*container class*/
	struct FileName
	{
		std::u8string name;
		uint32_t offset = 0;	//relative to FileName_offset
	};
	struct File
	{
		FileInfoEntry* fileInfoEntry = nullptr;
		FileName* filename = nullptr;

		stream::OptionalOwnerBuffer fileDataHeader;
		const FileDataHeader* getFileDataHeader(void) const
		{
			return reinterpret_cast<const FileDataHeader*>(fileDataHeader.get_const());
		}
		stream::OptionalOwnerBuffer compressedData;
		stream::OptionalOwnerBuffer decompressedData;
	};
}
namespace archive
{
	struct ArchiveInternal
	{
		Archive::Mode mode;
		stream::CipherStream stream;

		ArchiveHeader archiveHeader;
		SectionHeader sectionHeader;
		std::vector<TocEntry> tocList;
		std::vector<FolderEntry> folderList;
		std::vector<FileInfoEntry> fileInfoList;
		std::list<FileName> fileNameList;
		std::list<FileName> folderNameList;

		std::unordered_map<uint32_t, FileName*> fileNameLookUpTable;

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
			std::filesystem::path filepath = path / f.getFileDataHeader()->fileName;
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
					[f = std::move(f), c = std::move(c)]() mutable
				{
					f.decompressedData = c.get();
					return std::move(f);
				});
			}
			return std::make_tuple(std::move(r), filepath);
		}
		std::list<std::tuple<std::future<File>, std::filesystem::path>>
			extractFolder(
				ThreadPool& pool,
				const std::filesystem::path& root,
				uint16_t folderIndexl)
		{
			std::list<std::tuple<std::future<File>, std::filesystem::path>> r;
			FolderEntry f;
			// on linux, we should use '/' in directory instead of '\\'
#if !defined(_WIN32)
			std::replace(
				fileNameLookUpTable[f.fileNameOffset]->name.begin(),
				fileNameLookUpTable[f.fileNameOffset]->name.end(),
				u8'\\', u8'/');
#endif
			std::filesystem::path filepath =
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
					<< std::endl;
			}
			for (uint16_t i = fo.firstSubFolderIndex; i < fo.lastSubFolderIndex; ++i)
			{
				listFolder(i);
			}
		}
		std::future<File>
			buildFile(
				ThreadPool& pool,
				BuildFileTask& task,
				FileInfoEntry& entry,
				int compress_level)
		{
			File f;
			f.fileInfoEntry = &entry;
			f.decompressedData = std::vector<std::byte>(entry.decompressedLen);
			{
				std::ifstream ifile(task.realpath, std::ios::binary);
				assert(ifile);
				ifile.read(reinterpret_cast<char*>(f.decompressedData.get()),
					entry.decompressedLen);
			}
			f.fileDataHeader = std::vector<std::byte>(sizeof(FileDataHeader));
			auto h = reinterpret_cast<FileDataHeader*>(f.fileDataHeader.get());
			*h = FileDataHeader{ 0 };
			h->CRC = crc32(crc32(0, nullptr, 0),
				reinterpret_cast<const Bytef*>(f.decompressedData.get_const()),
				entry.decompressedLen);
			memmove(h->fileName,
				task.name.c_str(),
				(std::min)(sizeof(FileDataHeader::fileName) - 1, task.name.length()));
			h->modificationDate = uint32_t(
				std::chrono::duration_cast<std::chrono::seconds>(
					last_write_time(task.realpath).time_since_epoch()).count());
			if (task.name == u8"\x5f\xb4\xcb\xb4\xa6\xbd\xfb\xd6\xb9\xcd\xa8\xd0\xd0" ||
				task.name == u8"\x5f\xe6\xad\xa4\xe5\xa4\x84\xe7\xa6\x81\xe6\xad\xa2\xe9\x80\x9a\xe8\xa1\x8c")
			{
				std::cout << "Hello there!\n";
				task.compressMethod = Decompress_All_At_Once;
				constexpr size_t compressedLen = 1024;
				f.compressedData = std::vector<std::byte>(compressedLen);
				memset(f.compressedData.get(), 25, compressedLen);
				entry.compressedLen = compressedLen;
				entry.compressMethod = Decompress_All_At_Once;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			else if (Uncompressed == entry.compressMethod)
			{
				f.compressedData = std::move(f.decompressedData);
				entry.compressedLen = entry.decompressedLen;
				return std::async(std::launch::deferred,
					[f = std::move(f)]() mutable{return std::move(f); });
			}
			else
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
					return std::move(f);
				});
			}
		}
		std::list<std::future<File>>
			buildFolder(
				ThreadPool& pool,
				BuildFolderTask& task,
				FolderEntry& entry,
				int compress_level)
		{
			std::list<std::future<File>> l;
			for (uint16_t i = 0; i < task.subFileTasks.size(); i++)
			{
				l.emplace_back(buildFile(pool,
					task.subFileTasks[i],
					fileInfoList[entry.firstFileIndex + i],
					compress_level));
			}
			for (uint16_t i = 0; i < task.subFolderTasks.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.subFolderTasks[i],
					folderList[entry.firstSubFolderIndex + i],
					compress_level));
			}
			return l;
		}
		void preBuildFolder(BuildFolderTask& folderTask, uint16_t folderIndex)
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
			folderList[folderIndex].firstSubFolderIndex = uint16_t(folderList.size());
			folderList.resize(folderList.size() + folderTask.subFolderTasks.size());
			folderList[folderIndex].lastSubFolderIndex = uint16_t(folderList.size());
			folderList[folderIndex].firstFileIndex = uint16_t(fileInfoList.size());
			for (BuildFileTask& subFileTask : folderTask.subFileTasks)
			{
				FileName fileName;
				fileName.name = subFileTask.name;
				if (fileNameList.empty())
				{
					fileName.offset = 0;
				}
				else
				{
					fileName.offset = uint32_t(fileNameList.back().offset +
						fileNameList.back().name.length() + 1);
				}
				fileNameList.push_back(fileName);

				FileInfoEntry fileInfoEntry;
				fileInfoEntry.fileNameOffset = fileName.offset;
				fileInfoEntry.compressMethod = subFileTask.compressMethod;
				fileInfoEntry.decompressedLen = subFileTask.filesize;
				//data to be fill later:
				fileInfoEntry.fileDataOffset = 0;
				fileInfoEntry.compressedLen = 0;

				fileInfoList.push_back(fileInfoEntry);
			}
			folderList[folderIndex].lastFileIndex = uint16_t(fileInfoList.size());
			for (uint16_t i = 0; i < folderTask.subFolderTasks.size(); ++i)
			{
				preBuildFolder(folderTask.subFolderTasks[i],
					folderList[folderIndex].firstSubFolderIndex + i);
			}
		}
		// all nonsense around letter case are dealt in this function
		BuildArchiveTask
			parseTask(
				const buildfile::Archive& archive,
				const std::filesystem::path& root,
				std::vector<std::u8string> ignore_list)
		{
			//std::cout << "Parsing Build Config..." << std::endl;
			for (auto& ignore : ignore_list)
			{
				boost::to_lower(ignore);
			}
			BuildArchiveTask task;
			task.name = boost::to_lower_copy(archive.name);
			for (auto& toc : archive.TOCs)
			{
				BuildTOCTask tocTask;
				tocTask.name = boost::to_lower_copy(toc.param.name);
				tocTask.alias = boost::to_lower_copy(toc.param.alias);
				tocTask.rootFolderTask.name = u8"";

				//on linux, we should use '/' in relativeroot instead of '\\'
#if defined(_WIN32)
				auto tocRootPath = absolute(root / toc.param.relativeroot);
#else
				auto relativeroot_copy = toc.param.relativeroot;
				std::replace(relativeroot_copy.begin(), relativeroot_copy.end(), u8'\\', u8'/');
				auto tocRootPath = absolute(root / relativeroot_copy);
#endif
				

				for (auto& file : toc.files)
				{
					BuildFileTask fileTask;
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
						fileTask.realpath.lexically_relative(tocRootPath).remove_filename();

					BuildFolderTask* currentFolder = &tocTask.rootFolderTask;
					std::filesystem::path currentPath("");
					for (auto iter = relativePath.begin(); iter != relativePath.end(); ++iter)
					{
						currentPath /= *iter;

						bool found = false;
						for (BuildFolderTask& subFolderTask : currentFolder->subFolderTasks)
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
							BuildFolderTask newFolder;
							newFolder.name = boost::to_lower_copy(iter->u8string());
							newFolder.path_relative_to_root = boost::to_lower_copy(currentPath.u8string());
							currentFolder->subFolderTasks.push_back(newFolder);
							currentFolder = &currentFolder->subFolderTasks.back();
						}
					}
					currentFolder->subFileTasks.push_back(fileTask);
				}
				task.buildTOCTasks.push_back(tocTask);
			}
			return task;
		}
		std::future<void>
			buildTask(
				BuildArchiveTask task,
				ThreadPool& pool,
				int compress_level,
				bool skip_tool_signature)
		{
		//print basic information
		//std::cout << "Using " << _coreNum << " threads." << std::endl;
		//std::cout << "Compress Level: " << _compressLevel << std::endl;

		//std::cout << "Writing Archive Header..." << std::endl;
			archiveHeader = ArchiveHeader{ 0 };
			memmove(archiveHeader._ARCHIVE, "_ARCHIVE", sizeof(ArchiveHeader::_ARCHIVE));
			archiveHeader.version = 2;
			std::u16string tmpU16str =
				boost::locale::conv::utf_to_utf<char16_t>(task.name);

			memmove(archiveHeader.archiveName,
				tmpU16str.c_str(),
				(std::min)(sizeof(ArchiveHeader::archiveName) - sizeof(char16_t),
					tmpU16str.length() * sizeof(char16_t)));
			stream.write(&archiveHeader, sizeof(ArchiveHeader));

			sectionHeader = SectionHeader{ 0 };

			//std::cout << "Generating File Data..." << std::endl;
			for (auto& tocTask : task.buildTOCTasks)
			{
				TocEntry tocEntry;
				memset(&tocEntry, 0, sizeof(TocEntry));
				memmove(&tocEntry.name, tocTask.name.c_str(),
					(std::min)(sizeof(TocEntry::name) - 1, tocTask.name.length()));
				memmove(&tocEntry.alias, tocTask.alias.c_str(),
					(std::min)(sizeof(TocEntry::alias) - 1, tocTask.alias.length()));

				tocEntry.firstFolderIndex = uint16_t(folderList.size());
				tocEntry.firstFileIndex = uint16_t(fileInfoList.size());
				tocEntry.startHierarchyFolderIndex = uint16_t(folderList.size());

				folderList.emplace_back();
				preBuildFolder(tocTask.rootFolderTask, tocEntry.firstFolderIndex);

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

			//std::cout << "Writing Section Header..." << std::endl;
			const size_t sectionHeaderBegPos = stream.getpos();
			stream.write(&sectionHeader, sizeof(SectionHeader));
			sectionHeader.TOC_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));
			sectionHeader.TOC_count = uint16_t(tocList.size());
			for (TocEntry& entry : tocList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.Folder_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));
			sectionHeader.Folder_count = uint16_t(folderList.size());
			for (FolderEntry& entry : folderList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.FileInfo_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));
			sectionHeader.FileInfo_count = uint16_t(fileInfoList.size());
			for (FileInfoEntry& entry : fileInfoList)
			{
				stream.write(&entry, sizeof(entry));
			}
			sectionHeader.FileName_offset =
				uint32_t(stream.getpos() - sizeof(ArchiveHeader));
			sectionHeader.FileName_count = uint16_t(folderNameList.size());
			for (FileName& filename : folderNameList)
			{
				stream.write(filename.name.c_str(), filename.name.length() + 1);
			}

			archiveHeader.sectionHeaderSize =
				uint32_t(stream.getpos() - sectionHeaderBegPos);

			std::list<std::future<File>> l;
			for (uint16_t i = 0; i < task.buildTOCTasks.size(); i++)
			{
				l.splice(l.end(), buildFolder(pool,
					task.buildTOCTasks[i].rootFolderTask,
					folderList[tocList[i].startHierarchyFolderIndex],
					compress_level));
			}

			archiveHeader.exactFileDataOffset = uint32_t(stream.getpos());

			//std::cout << "Writing Compressed Files..." << std::endl;
			if (Archive::Write_Encrypted == mode)
			{
				stream.writeKey();
			}
			return std::async(std::launch::async,
				[this, skip_tool_signature, l = std::move(l)]() mutable
			{
				for (auto& filefuture : l)
				{
					auto file = filefuture.get();
					stream.write(file.getFileDataHeader(), sizeof(FileDataHeader));
					file.fileInfoEntry->fileDataOffset =
						uint32_t(stream.getpos() - archiveHeader.exactFileDataOffset);
					stream.write(file.compressedData.get_const(),
						file.fileInfoEntry->compressedLen);
				}

				//std::cout << "Rewrite Section Header..." << std::endl;
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
				//std::cout << "Calculating Archive Signature..." << std::endl;
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
					//std::cout << "Tool Signature Calculation Skipped." << std::endl;
				}
				else
				{
					//calculate toolSignature
					//std::cout << "Calculating Tool Signature..." << std::endl;
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
				//std::cout << "Rewrite Archive Header..." << std::endl;
				stream.setpos(0);
				stream.write(&archiveHeader, sizeof(ArchiveHeader));

				if (Archive::Write_Encrypted == mode)
				{
					stream.writeEncryptionEnd();
				}
				//std::cout << std::endl;
				//std::cout << "Build Finished." << std::endl;
			});
		}
	};
	Archive::Archive()
	{
		_internal = std::unique_ptr<ArchiveInternal>(new ArchiveInternal);
	}
	void Archive::open(
		const std::filesystem::path& filepath, Mode mode)
	{
		_internal->archiveHeader = ArchiveHeader{ 0 };
		_internal->sectionHeader = SectionHeader{ 0 };
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
		default: // Write_Encrypted
		{
			_internal->stream.open(filepath, stream::Write_Encrypted);
		}
		break;
		}
		_internal->mode = mode;
	}
	std::future<void> Archive::extract(ThreadPool& pool, const std::filesystem::path& root)
	{
		assert(Read == _internal->mode);
		assert(create_directories(root));
		std::list<std::tuple<std::future<File>, std::filesystem::path>> files;
		for (const TocEntry& toc : _internal->tocList)
		{
			auto r = _internal->extractFolder(pool, root / toc.name, toc.startHierarchyFolderIndex);
			files.splice(files.end(), r);
		}
		return std::async(std::launch::async,
			[files = std::move(files)]() mutable
		{
			for (auto& f : files)
			{
				const auto data = std::get<0>(f).get();
				const auto path = std::get<1>(f);
				create_directories(path.parent_path());
				{
					std::ofstream ofile(path, std::ios::binary);
					assert(ofile);
					ofile.write(
						reinterpret_cast<const char*>(data.decompressedData.get_const()),
						data.fileInfoEntry->decompressedLen);
				}
				last_write_time(path, std::filesystem::file_time_type() +
					std::chrono::seconds(data.getFileDataHeader()->modificationDate));
			}
		});
	}
	std::future<void> Archive::create(
		ThreadPool& pool,
		const buildfile::Archive& task,
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
	void Archive::listFiles()
	{
		assert(Read == _internal->mode);
		for (TocEntry& toc : _internal->tocList)
		{
			std::cout << "TOCEntry" << std::endl;
			std::cout << "  Name : '" << toc.name << "'" << std::endl;
			std::cout << "  Alias: '" << toc.alias << "'" << std::endl;
			std::cout << std::endl;
			std::cout << std::left << std::setw(72) << "File Name"
				<< std::right << std::setw(16) << "Original Size"
				<< std::right << std::setw(16) << "Stored Size"
				<< std::right << std::setw(8) << "Ratio"
				<< std::right << std::setw(16) << "Storage Type"
				<< std::endl;
			_internal->listFolder(toc.startHierarchyFolderIndex);
		}
		std::cout << std::endl;
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
	std::u8string Archive::getArchiveSignature()
	{
		return reinterpret_cast<char8_t*>(_internal->archiveHeader.archiveSignature);
	}
	void Archive::close()
	{
		_internal->stream.close();
	}
}
