#include "stdafx.h"

#include "HWRM_BigFile_Internal.h"

void BigFile_Internal::open(boost::filesystem::path file, BigFileState state)
{
	_state = state;
	switch (state)
	{
	case read:
	{
		_tocList.clear();
		_folderList.clear();
		_fileInfoList.clear();
		_fileNameList.clear();
		_fileNameLookUpTable.clear();

		_cipherStream.open(file, Read_EncryptionUnknown);
		_cipherStream.read(&_archiveHeader, sizeof(_archiveHeader));
		_cipherStream.read(&_sectionHeader, sizeof(_sectionHeader));

		_cipherStream.setpos(sizeof(ArchiveHeader) + _sectionHeader.TOC_offset);
		for (uint16_t i = 0; i < _sectionHeader.TOC_count; ++i)
		{
			_tocList.push_back(TocEntry());
			_cipherStream.read(&_tocList.back(), sizeof(TocEntry));
		}

		_cipherStream.setpos(sizeof(ArchiveHeader) + _sectionHeader.Folder_offset);
		for (uint16_t i = 0; i < _sectionHeader.Folder_count; ++i)
		{
			_folderList.push_back(FolderEntry());
			_cipherStream.read(&_folderList.back(), sizeof(FolderEntry));
		}

		_cipherStream.setpos(sizeof(ArchiveHeader) + _sectionHeader.FileInfo_offset);
		for (uint16_t i = 0; i < _sectionHeader.FileInfo_count; ++i)
		{
			_fileInfoList.push_back(FileInfoEntry());
			_cipherStream.read(&_fileInfoList.back(), sizeof(FileInfoEntry));
		}

		size_t filenameOffset = sizeof(ArchiveHeader) + _sectionHeader.FileName_offset;
		_cipherStream.setpos(filenameOffset);
		for (uint16_t i = 0; i < _sectionHeader.FileName_count; ++i)
		{
			_fileNameList.push_back(FileName());
			_fileNameList.back().offset = uint32_t(_cipherStream.getpos() - filenameOffset);
			_fileNameList.back().name = "";
			char c;
			while (true)
			{
				_cipherStream.read(&c, 1);
				if (!c)
					break;
				_fileNameList.back().name += c;
			}
		}
		for (FileName& filename : _fileNameList)
		{
			_fileNameLookUpTable[filename.offset] = &filename;
		}
	}
	break;
	case write:
	{
		_tocList.clear();
		_folderList.clear();
		_fileInfoList.clear();
		_fileNameList.clear();
		_fileNameLookUpTable.clear();

		_cipherStream.open(file, Write_NonEncrypted);
	}
	break;
	default:
		throw OutOfRangeError();
	}
}

void BigFile_Internal::close()
{
	_cipherStream.close();
}

static void outputPercentBar(int percent);
void BigFile_Internal::extract(boost::filesystem::path directory)
{
	if (_state != read)
	{
		throw OutOfRangeError();
	}

	if (!exists(directory))
	{
		if (!create_directories(directory))
		{
			throw FileIoError("Error happened when create directory.");
		}
	}
	else if (!is_directory(directory))
	{
		throw FileIoError("Specified path is not a directory.");
	}

	unsigned coreNum = std::thread::hardware_concurrency();
	if (coreNum < 2)
	{
		coreNum = 2;
	}

	_threadPool = std::make_unique<ThreadPool>(coreNum);
	{
		std::queue<std::future<void>> empty;
		swap(_futureList, empty);
	}
	_progress = "";
	{
		std::queue<std::string> empty;
		swap(_errorList, empty);
	}

	for (TocEntry &toc : _tocList)
	{
		extractFolder(directory / toc.name, toc.startHierarchyFolderIndex);
	}

	std::cout << "Extracting..." << std::endl;
	bool flagUnFinished = true;
	uint32_t finishedFilesNum = 0;
	while (flagUnFinished)
	{
		flagUnFinished = false;
		while (!_futureList.empty())
		{
			std::future_status status = _futureList.front().wait_for(std::chrono::seconds(0));
			if (status != std::future_status::ready)
			{
				flagUnFinished = true;
				break;
			}
			else
			{
				++finishedFilesNum;
				_futureList.pop();
			}
		}
		while (!_errorList.empty())
		{
			_errorMutex.lock();
			std::string tmpStr = _errorList.front();
			_errorList.pop();
			_errorMutex.unlock();

			std::cerr << '\r' << tmpStr;
			for (auto i = tmpStr.length(); i < 70; ++i)
			{
				std::cout << ' ';
			}
			std::cout << std::endl;
		}
		int percent = int(finishedFilesNum * 100 / _fileInfoList.size());
		outputPercentBar(percent);

		std::string tmpStr = _progress;
		if (tmpStr.length() > 50)
		{
			std::cout << tmpStr.substr(0, 47) << "...";
		}
		else
		{
			std::cout << tmpStr << std::string(50 - tmpStr.length(), ' ');
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	outputPercentBar(100);
	std::cout << std::string(50, ' ') << std::endl;
	std::cout << "Finished!" << std::endl;
}

void BigFile_Internal::listFiles()
{
	if (_state != read)
	{
		throw OutOfRangeError();
	}

	for (TocEntry &toc : _tocList)
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

		listFolder(toc.startHierarchyFolderIndex);
	}

	std::cout << std::endl;
}

void BigFile_Internal::testArchive()
{
	if (_state != read)
	{
		throw OutOfRangeError();
	}

	unsigned coreNum = std::thread::hardware_concurrency();
	if (coreNum < 2)
	{
		coreNum = 2;
	}

	_threadPool = std::make_unique<ThreadPool>(coreNum);
	{
		std::queue<std::future<void>> empty;
		swap(_futureList, empty);
	}
	_progress = "";
	{
		std::queue<std::string> empty;
		swap(_errorList, empty);
	}
	_testPassed = true;

	for (TocEntry &toc : _tocList)
	{
		testFolder(toc.startHierarchyFolderIndex);
	}

	std::cout << "Archive Self Testing" << std::endl;
	std::cout << "TEST: RUNNING - Archive Self Integrity Check" << std::endl;
	bool flagUnFinished = true;
	uint32_t finishedFilesNum = 0;
	while (flagUnFinished)
	{
		flagUnFinished = false;
		while (!_futureList.empty())
		{
			std::future_status status = _futureList.front().wait_for(std::chrono::seconds(0));
			if (status != std::future_status::ready)
			{
				flagUnFinished = true;
				break;
			}
			else
			{
				++finishedFilesNum;
				_futureList.pop();
			}
		}
		while (!_errorList.empty())
		{
			_errorMutex.lock();
			std::string tmpStr = _errorList.front();
			_errorList.pop();
			_errorMutex.unlock();

			std::cerr << '\r' << tmpStr;
			for (auto i = tmpStr.length(); i < 70; ++i)
			{
				std::cout << ' ';
			}
			std::cout << std::endl;
		}
		int percent = int(finishedFilesNum * 100 / _fileInfoList.size());
		outputPercentBar(percent);

		std::string tmpStr = _progress;
		if (tmpStr.length() > 50)
		{
			std::cout << tmpStr.substr(0, 47) << "...";
		}
		else
		{
			std::cout << tmpStr << std::string(50 - tmpStr.length(), ' ');
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	outputPercentBar(100);
	std::cout << std::string(50, ' ') << std::endl;
	std::cout << "TEST: " << (_testPassed ? "PASSED" : "FAILED")
		<< "  - Archive Self Integrity Check" << std::endl;
}

void BigFile_Internal::build(BuildArchiveTask task)
{
	memcpy(_archiveHeader._ARCHIVE, "_ARCHIVE", 8);
	_archiveHeader.version = 2;
	//toolSignature
	std::wstring tmpWstr = boost::locale::conv::to_utf<wchar_t>(task.name, std::locale(""));
	memset(_archiveHeader.archiveName, 0, 128);
	memcpy(_archiveHeader.archiveName, tmpWstr.c_str(), std::min(size_t(63), tmpWstr.length()) * 2);
	//archiveSignature
	//exactFileDataOffset
	_cipherStream.write(&_archiveHeader, sizeof(_archiveHeader));
	memset(&_sectionHeader, 0, sizeof(_sectionHeader));

	for(BuildTOCTask &tocTask : task.buildTOCTasks)
	{
		TocEntry tocEntry;
		memset(&tocEntry, 0, sizeof(tocEntry));
		memcpy(&tocEntry.name, tocTask.name.c_str(), std::min(size_t(63), tocTask.name.length()));
		memcpy(&tocEntry.alias, tocTask.alias.c_str(), std::min(size_t(63), tocTask.alias.length()));
		
		tocEntry.firstFolderIndex = uint16_t(_folderList.size());
		tocEntry.firstFileIndex = uint16_t(_fileInfoList.size());
		tocEntry.startHierarchyFolderIndex = uint16_t(_folderList.size());
		
		_folderList.push_back(FolderEntry());
		preBuildFolder(tocTask.rootFolderTask, _folderList.back());

		tocEntry.lastFolderIndex = uint16_t(_folderList.size());
		tocEntry.lastFileIndex = uint16_t(_fileInfoList.size());
		_tocList.push_back(tocEntry);
	}


	size_t sectionHeaderBegPos = _cipherStream.getpos() - sizeof(_archiveHeader);
	_cipherStream.write(&_sectionHeader, sizeof(_sectionHeader));
	_sectionHeader.TOC_offset = uint32_t(_cipherStream.getpos());
	_sectionHeader.TOC_count = uint16_t(_tocList.size());
	for (TocEntry &entry : _tocList)
	{
		_cipherStream.write(&entry, sizeof(entry));
	}
	_sectionHeader.Folder_offset = uint32_t(_cipherStream.getpos() - sizeof(_archiveHeader));
	_sectionHeader.Folder_count = uint16_t(_folderList.size());
	for (FolderEntry &entry : _folderList)
	{
		_cipherStream.write(&entry, sizeof(entry));
	}
	_sectionHeader.FileInfo_offset = uint32_t(_cipherStream.getpos() - sizeof(_archiveHeader));
	_sectionHeader.FileInfo_count = uint16_t(_fileInfoList.size());
	for (FileInfoEntry &entry : _fileInfoList)
	{
		_cipherStream.write(&entry, sizeof(entry));
	}
	_sectionHeader.FileName_offset = uint32_t(_cipherStream.getpos() - sizeof(_archiveHeader));
	_sectionHeader.FileName_count = uint16_t(_fileNameList.size());
	for (FileName &filename : _fileNameList)
	{
		_cipherStream.write(filename.name.c_str(), filename.name.length() + 1);
	}

	_archiveHeader.sectionHeaderSize = uint32_t(_cipherStream.getpos() - sectionHeaderBegPos);

	for(uint16_t i=0;i<task.buildTOCTasks.size();++i)
	{
		buildFolder(
			task.buildTOCTasks[i].rootFolderTask,
			_folderList[_tocList[i].startHierarchyFolderIndex]
		);
	}
}

const uint8_t* BigFile_Internal::getArchiveSignature() const
{
	return _archiveHeader.archiveSignature;
}

void BigFile_Internal::extractFolder(boost::filesystem::path directory, uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstSubFolderIndex; i < thisfolder.lastSubFolderIndex; ++i)
	{
		extractFolder(directory, i);
	}

	directory /= _fileNameLookUpTable[thisfolder.fileNameOffset]->name;
	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		_futureList.push(
			_threadPool->enqueue(&BigFile_Internal::extractFile, this, directory, i)
		);
	}
}

class ZlibError {};

void BigFile_Internal::extractFile(boost::filesystem::path directory, uint16_t fileIndex)
{

	File thisfile;
	thisfile.fileInfoEntry = &_fileInfoList[fileIndex];
	size_t pos = _archiveHeader.exactFileDataOffset + thisfile.fileInfoEntry->fileDataOffset;
	thisfile.fileDataHeader = _cipherStream.thread_readProxy(
		pos - sizeof(FileDataHeader), sizeof(FileDataHeader)
	);
	thisfile.data = _cipherStream.thread_readProxy(pos, thisfile.fileInfoEntry->compressedLen);
	boost::filesystem::path filepath = directory / thisfile.getFileDataHeader()->fileName;

	try
	{
		//解压
		//无压缩文件的“解压后数据”直接来自“压缩后数据”
		if (thisfile.fileInfoEntry->compressMethod == Uncompressed)
		{
			thisfile.decompressedData = move(thisfile.data);
		}
		else
		{
			uLongf outLen = thisfile.fileInfoEntry->decompressedLen;
			char *tmpData = new char[outLen];
			int zRet = uncompress(
				reinterpret_cast<Bytef*>(tmpData),
				&outLen,
				reinterpret_cast<const Bytef*>(thisfile.data->data),
				thisfile.fileInfoEntry->compressedLen
			);
			if (zRet != Z_OK || outLen != thisfile.fileInfoEntry->decompressedLen)
			{
				throw ZlibError();
			}
			thisfile.decompressedData = std::make_unique<readDataProxy>(true);
			thisfile.decompressedData->data = tmpData;
		}
		create_directories(directory);

		boost::filesystem::ofstream ofile(filepath, std::ios::binary);
		if (!ofile.is_open())
		{
			throw FileIoError("Error happened when openning file for output.");
		}
		ofile.write(thisfile.decompressedData->data, thisfile.fileInfoEntry->decompressedLen);
		ofile.close();

		last_write_time(filepath, thisfile.getFileDataHeader()->modificationDate);

		_progressMutex.lock();
		_progress = thisfile.getFileDataHeader()->fileName;
		_progressMutex.unlock();
	}
	catch (ZlibError)
	{
		_errorMutex.lock();
		_errorList.push("Failed to Decompress file: " + filepath.generic_string());
		_errorMutex.unlock();
	}
}

void BigFile_Internal::preBuildFolder(BuildFolderTask& folderTask, FolderEntry& folderEntry)
{
	FileName folderName;
	folderName.name = folderTask.fullpath;
	if (_fileNameList.empty())
	{
		folderName.offset = 0;
	}
	else
	{
		folderName.offset =
			uint32_t(_fileNameList.back().offset + _fileNameList.back().name.length() + 1);
	}
	_fileNameList.push_back(folderName);
	folderEntry.fileNameOffset = folderName.offset;
	folderEntry.firstSubFolderIndex = uint16_t(_folderList.size());
	for (BuildFolderTask &subFolderTask : folderTask.subFolderTasks)
	{
		_folderList.push_back(FolderEntry());
	}
	folderEntry.lastSubFolderIndex = uint16_t(_folderList.size());
	folderEntry.firstFileIndex = uint16_t(_fileInfoList.size());
	for (BuildFileTask &subFileTask : folderTask.subFileTasks)
	{
		FileInfoEntry fileInfoEntry;
		FileName fileName;
		fileName.name = subFileTask.name;
		fileName.offset =
			uint32_t(_fileNameList.back().offset + _fileNameList.back().name.length() + 1);
		_fileNameList.push_back(fileName);
		fileInfoEntry.fileNameOffset = fileName.offset;
		fileInfoEntry.compressMethod = subFileTask.compressMethod;
		fileInfoEntry.decompressedLen = subFileTask.filesize;
		//fileDataOffset;	
		//compressedLen;
		_fileInfoList.push_back(fileInfoEntry);
	}
	folderEntry.lastFileIndex = uint16_t(_fileInfoList.size());
	for (uint16_t i = 0; i < folderTask.subFolderTasks.size(); ++i)
	{
		preBuildFolder(
			folderTask.subFolderTasks[i],
			_folderList[folderEntry.firstSubFolderIndex + i]
		);
	}
}

void BigFile_Internal::buildFolder(BuildFolderTask& folderTask, FolderEntry& folderEntry)
{

	for (uint16_t i = 0; i < folderTask.subFileTasks.size(); ++i)
	{
		_futureFileList.push(
			_threadPool->enqueue(
				&BigFile_Internal::buildFile,
				this, folderTask.subFileTasks[i], _fileInfoList[folderEntry.firstFileIndex + i]
			)
		);
	}
	for (uint16_t i = 0; i < folderTask.subFolderTasks.size(); ++i)
	{
		buildFolder(
			folderTask.subFolderTasks[i],
			_folderList[folderEntry.firstSubFolderIndex + i]
		);
	}
}

std::unique_ptr<File> BigFile_Internal::buildFile(BuildFileTask& fileTask, FileInfoEntry& fileInfoEntry)
{
	std::unique_ptr<File> file(new File);
	file->fileInfoEntry = &fileInfoEntry;
	char *decompressedData = new char[fileInfoEntry.decompressedLen];
	boost::filesystem::ifstream ifile(fileTask.realpath, std::ios::binary);
	if (!ifile.is_open())
	{
		throw FileIoError("Error happened when openning file to compress.");
	}
	ifile.read(decompressedData, fileInfoEntry.decompressedLen);
	ifile.close();
	file->decompressedData = std::make_unique<readDataProxy>(true);
	file->decompressedData->data = decompressedData;
	decompressedData = nullptr;
	
	char* fileDataHeader_charArray = new char[sizeof(FileDataHeader)];
	memset(fileDataHeader_charArray, 0, sizeof(FileDataHeader));
	FileDataHeader *fileDataHeader = reinterpret_cast<FileDataHeader*>(fileDataHeader_charArray);
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(
		crc,
		reinterpret_cast<const Bytef*>(file->decompressedData->data),
		fileInfoEntry.decompressedLen
	);
	fileDataHeader->CRC = crc;
	memcpy(
		fileDataHeader->fileName,
		fileTask.name.c_str(), std::min(size_t(255), fileTask.name.length() + 1)
	);
	fileDataHeader->modificationDate = uint32_t(
		boost::filesystem::detail::last_write_time(fileTask.realpath)
	);
	file->fileDataHeader = std::make_unique<readDataProxy>(true);
	file->fileDataHeader->data = fileDataHeader_charArray;
	fileDataHeader_charArray = nullptr;
	fileDataHeader = nullptr;

	if (fileInfoEntry.compressMethod != Uncompressed)
	{
		uLong compressedLen = compressBound(fileInfoEntry.decompressedLen);
		char* compressedData = new char[compressedLen];
		int zRet = compress(
			reinterpret_cast<Bytef*>(compressedData),
			&compressedLen,
			reinterpret_cast<const Bytef*>(file->data->data),
			fileInfoEntry.decompressedLen
		);
		if (zRet != Z_OK)
		{
			throw FileIoError("Error happened when compressing file.");
		}
		file->data = std::make_unique<readDataProxy>(true);
		file->data->data = compressedData;
		compressedData = nullptr;

		fileInfoEntry.compressedLen = compressedLen;
	}
	else
	{
		file->data = move(file->decompressedData);
		fileInfoEntry.compressedLen = fileInfoEntry.decompressedLen;
	}

	return file;
}

void BigFile_Internal::listFolder(uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		FileInfoEntry &thisfile = _fileInfoList[i];

		std::string filename = (
			boost::filesystem::path(
				_fileNameLookUpTable[thisfolder.fileNameOffset]->name
			) /
			_fileNameLookUpTable[thisfile.fileNameOffset]->name
			).generic_string();

		double ratio = double(thisfile.compressedLen) / double(thisfile.decompressedLen);
		std::string storageType;
		switch (thisfile.compressMethod)
		{
		case Uncompressed:
			storageType = "Store";
			break;
		case Decompress_During_Read:
			storageType = "Compress Stream";
			break;
		case Decompress_All_At_Once:
			storageType = "Compress Buffer";
			break;
		default:
			throw OutOfRangeError();
		}

		std::cout << std::left << std::setw(72)
			<< "  " + filename
			<< std::right << std::setw(16)
			<< thisfile.decompressedLen
			<< std::right << std::setw(16)
			<< thisfile.compressedLen
			<< std::right << std::fixed << std::setw(8) << std::setprecision(3)
			<< ratio
			<< std::right << std::setw(16)
			<< storageType
			<< std::endl;
	}
	for (uint16_t i = thisfolder.firstSubFolderIndex; i < thisfolder.lastSubFolderIndex; ++i)
	{
		listFolder(i);
	}
}

void BigFile_Internal::testFolder(uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstSubFolderIndex; i < thisfolder.lastSubFolderIndex; ++i)
	{
		testFolder(i);
	}

	boost::filesystem::path path(
		_fileNameLookUpTable[thisfolder.fileNameOffset]->name
	);
	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		_futureList.push(
			_threadPool->enqueue(&BigFile_Internal::testFile, this, path, i)
		);
	}
}

void BigFile_Internal::testFile(boost::filesystem::path path, uint16_t fileIndex)
{
	File thisfile;
	thisfile.fileInfoEntry = &_fileInfoList[fileIndex];
	size_t pos = _archiveHeader.exactFileDataOffset + thisfile.fileInfoEntry->fileDataOffset;
	thisfile.fileDataHeader = _cipherStream.thread_readProxy(
		pos - sizeof(FileDataHeader), sizeof(FileDataHeader)
	);
	thisfile.data = _cipherStream.thread_readProxy(pos, thisfile.fileInfoEntry->compressedLen);
	boost::filesystem::path filepath = path / thisfile.getFileDataHeader()->fileName;

	try
	{
		//解压
		//无压缩文件的“解压后数据”直接来自“压缩后数据”
		if (thisfile.fileInfoEntry->compressMethod == Uncompressed)
		{
			thisfile.decompressedData = move(thisfile.data);
		}
		else
		{
			uLongf outLen = thisfile.fileInfoEntry->decompressedLen;
			char *tmpData = new char[outLen];
			int zRet = uncompress(
				reinterpret_cast<Bytef*>(tmpData),
				&outLen,
				reinterpret_cast<const Bytef*>(thisfile.data->data),
				thisfile.fileInfoEntry->compressedLen
			);
			if (zRet != Z_OK || outLen != thisfile.fileInfoEntry->decompressedLen)
			{
				throw ZlibError();
			}
			thisfile.decompressedData = std::make_unique<readDataProxy>(true);
			thisfile.decompressedData->data = tmpData;
		}

		//check crc
		uLong crc = crc32(0L, Z_NULL, 0);
		crc = crc32(
			crc,
			reinterpret_cast<const Bytef*>(thisfile.decompressedData->data),
			thisfile.fileInfoEntry->decompressedLen
		);
		if (crc != thisfile.getFileDataHeader()->CRC)
		{
			_testPassed = false;
			_errorMutex.lock();
			_errorList.push("File CRC mismatch: " + filepath.generic_string());
			_errorMutex.unlock();
		}

		_progressMutex.lock();
		_progress = thisfile.getFileDataHeader()->fileName;
		_progressMutex.unlock();
	}
	catch (ZlibError)
	{
		_testPassed = false;
		_errorMutex.lock();
		_errorList.push("Failed to decompress file: " + filepath.generic_string());
		_errorMutex.unlock();
	}
}


//UGLY BAR
static void outputPercentBar(int percent)
{
	if (percent < 5)
		std::cout << "\r[----------" << percent << "%--------]";
	else if (percent < 10)
		std::cout << "\r[#---------" << percent << "%--------]";
	else if (percent < 15)
		std::cout << "\r[##-------" << percent << "%--------]";
	else if (percent < 20)
		std::cout << "\r[###------" << percent << "%--------]";
	else if (percent < 25)
		std::cout << "\r[####-----" << percent << "%--------]";
	else if (percent < 30)
		std::cout << "\r[#####----" << percent << "%--------]";
	else if (percent < 35)
		std::cout << "\r[######---" << percent << "%--------]";
	else if (percent < 40)
		std::cout << "\r[#######--" << percent << "%--------]";
	else if (percent < 45)
		std::cout << "\r[########-" << percent << "%--------]";
	else if (percent < 50)
		std::cout << "\r[#########" << percent << "%--------]";
	else if (percent < 70)
		std::cout << "\r[#########" << percent << "%#-------]";
	else if (percent < 75)
		std::cout << "\r[#########" << percent << "%##------]";
	else if (percent < 80)
		std::cout << "\r[#########" << percent << "%###-----]";
	else if (percent < 85)
		std::cout << "\r[#########" << percent << "%####----]";
	else if (percent < 90)
		std::cout << "\r[#########" << percent << "%#####---]";
	else if (percent < 95)
		std::cout << "\r[#########" << percent << "%######--]";
	else if (percent < 100)
		std::cout << "\r[#########" << percent << "%#######-]";
	else
		std::cout << "\r[########100%########]";
}