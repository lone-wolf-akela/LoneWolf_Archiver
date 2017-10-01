#pragma once
#include "stdafx.h"

#include "filestream.h"
#include "memmapfilestream.h"
#include "cipherstream.h"

/*enum*/
enum CompressMethod : uint8_t
{
	Uncompressed = 0x00,
	Decompress_During_Read = 0x10,
	Decompress_All_At_Once = 0x20
};
/*task struct*/
struct BuildFileTask
{
	std::string name;
	boost::filesystem::path realpath;
	CompressMethod compressMethod;
	uint32_t filesize;
};
struct BuildFolderTask
{
	std::string name;
	std::vector<BuildFolderTask> subFolderTasks;
	std::vector<BuildFileTask> subFileTasks;
};
struct BuildTOCTask
{
	std::string name;
	std::string alias;
	BuildFolderTask rootFolderTask;
};
struct BuildArchiveTask
{
	std::string name;
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

/*data struct*/
#pragma pack (1)
struct ArchiveHeader
{
	char _ARCHIVE[8];
	uint32_t version;

	//E01519D6-2DB7-4640-AF54-0A23319C56C3 + 除Archive_Header外所有部分的数据的MD5
	uint8_t toolSignature[16];

	wchar_t archiveName[64];

	//DFC9AF62-FC1B-4180-BC27-11CCE87D3EFF + Archive_Header之后、exact file data之前的所有数据的MD5
	uint8_t archiveSignature[16];

	uint32_t sectionHeaderSize;
	uint32_t exactFileDataOffset;
};
struct SectionHeader
{
	//all offset relative to archive header
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
	uint16_t firstFileNameIndex;
	uint16_t lastFileNameIndex;
	uint16_t startHierarchyFolderIndex;
};
struct FolderEntry
{
	uint32_t fileNameOffset;		//relative to FileName_offset
	uint16_t firstSubFolderIndex;
	uint16_t lastSubFolderIndex;	//not included
	uint16_t firstFileNameIndex;
	uint16_t lastFileNameIndex;		//not included
};
struct FileInfoEntry
{
	uint32_t fileNameOffset;	//relative to FileName_offset
	CompressMethod compressMethod;
	uint32_t fileDataOffset;	//relative to overall file data offset
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
	uint32_t offset;	//relative to FileName_offset
};
struct File
{
	FileInfoEntry *fileInfoEntry = nullptr;	
	FileName* filename = nullptr;

	std::unique_ptr<readDataProxy> fileDataHeader;
	const FileDataHeader* getFileDataHeader(void) const
	{
		return reinterpret_cast<const FileDataHeader*>(fileDataHeader->data);
	}
	std::unique_ptr<readDataProxy> data;
	std::unique_ptr<readDataProxy> decompressedData;	
};

/*core class*/
enum BigFileState
{
	read,write
};

class BigFile_Internal
{
public:
	BigFile_Internal(void) = default;
	BigFile_Internal(boost::filesystem::path file, BigFileState state)
	{
		open(file, state);
	}
	~BigFile_Internal(void)
	{
		close();
	}
	void open(boost::filesystem::path file, BigFileState state);
	void close(void);

	void extract(boost::filesystem::path directory);
	void listFiles(void);
	void testArchive(void);
	void build(BuildArchiveTask task);

	const uint8_t* getArchiveSignature(void) const;

private:
	CipherStream _cipherStream;
	std::unique_ptr<ThreadPool> _threadPool;
	std::queue<std::future<void>> _futureList;
	std::string _progress;
	std::mutex _progressMutex;
	std::queue<std::string> _errorList;
	std::mutex _errorMutex;
	std::atomic_bool _testPassed;

	BigFileState _state;

	ArchiveHeader _archiveHeader;
	SectionHeader _sectionHeader;
	std::vector<TocEntry> _tocList;
	std::vector<FolderEntry> _folderList;
	std::vector<FileInfoEntry> _fileInfoList;
	std::vector<FileName> _fileNameList;
	std::unordered_map<uint32_t, FileName*> _fileNameLookUpTable;

	uint16_t _folderNum;

	void extractFolder(boost::filesystem::path directory, uint16_t folderIndex);
	void extractFile(boost::filesystem::path directory, uint16_t fileIndex);
	void listFolder(uint16_t folderIndex);
	void testFolder(uint16_t folderIndex);
	void testFile(boost::filesystem::path path, uint16_t fileIndex);
};