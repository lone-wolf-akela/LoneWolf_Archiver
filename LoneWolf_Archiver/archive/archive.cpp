#include "../stream/cipherstream.h"

#include "archive.h"

namespace
{
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
		std::u8string name;
		std::u8string fullpath;
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
		std::string name;
		uint32_t offset = 0;	//relative to FileName_offset
	};
	struct File
	{
		FileInfoEntry* fileInfoEntry = nullptr;
		FileName* filename = nullptr;

		OptionalOwnerBuffer fileDataHeader;
		const FileDataHeader* getFileDataHeader(void) const
		{
			return reinterpret_cast<const FileDataHeader*>(fileDataHeader.get());
		}
		OptionalOwnerBuffer data;
		OptionalOwnerBuffer decompressedData;
	};
}
namespace archive
{
	struct ArchiveInternal
	{

	};
	Archive::Archive()
	{
		_internal = std::unique_ptr<ArchiveInternal>(new ArchiveInternal);
	}
	void Archive::open(const std::filesystem::path& filepath, Mode mode, bool encryption)
	{
	}
	void Archive::extract(ThreadPool& pool, const std::filesystem::path& directory)
	{
	}
	void Archive::create(ThreadPool& pool, const std::filesystem::path& root, int compress_level, bool skip_tool_signature, std::vector<std::string> ignore_list)
	{
	}
	void Archive::listFiles()
	{
	}
	void Archive::testArchive()
	{
	}
	std::string Archive::getArchiveSignature()
	{
		return std::string();
	}
	void Archive::close()
	{
	}
}
