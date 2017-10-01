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

		_folderNum = _tocList.back().lastFolderIndex;
	}
	break;
	case write:
		throw NotImplementedError();
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
		while(!_futureList.empty())
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
		std::cout << std::left << std::setw(72)<< "File Name" 
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
	for (uint16_t i = thisfolder.firstFileNameIndex; i < thisfolder.lastFileNameIndex; ++i)
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

void BigFile_Internal::listFolder(uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstFileNameIndex; i < thisfolder.lastFileNameIndex; ++i)
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
	for (uint16_t i = thisfolder.firstFileNameIndex; i < thisfolder.lastFileNameIndex; ++i)
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