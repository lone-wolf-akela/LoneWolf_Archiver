#include "stdafx.h"

#include "HWRM_BigFile_Internal.h"
#include "linuxfix.h"

void BigFile_Internal::open(std::filesystem::path file, BigFileState state)
{
	_state = state;
	switch (state)
	{
	case Read:
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
			_tocList.emplace_back();
			_cipherStream.read(&_tocList.back(), sizeof(TocEntry));
		}

		_cipherStream.setpos(sizeof(ArchiveHeader) + _sectionHeader.Folder_offset);
		for (uint16_t i = 0; i < _sectionHeader.Folder_count; ++i)
		{
			_folderList.emplace_back();
			_cipherStream.read(&_folderList.back(), sizeof(FolderEntry));
		}

		_cipherStream.setpos(sizeof(ArchiveHeader) + _sectionHeader.FileInfo_offset);
		for (uint16_t i = 0; i < _sectionHeader.FileInfo_count; ++i)
		{
			_fileInfoList.emplace_back();
			_cipherStream.read(&_fileInfoList.back(), sizeof(FileInfoEntry));
		}

		const size_t filenameOffset = sizeof(ArchiveHeader) + _sectionHeader.FileName_offset;
		_cipherStream.setpos(filenameOffset);
		for (uint16_t i = 0; i < _sectionHeader.FileName_count; ++i)
		{
			_fileNameList.emplace_back();
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
	case Write:
	{
		_tocList.clear();
		_folderList.clear();
		_fileInfoList.clear();
		_fileNameList.clear();
		_folderNameList.clear();
		_fileNameLookUpTable.clear();

		if (_writeEncryption)
		{
			_cipherStream.open(file, Write_Encrypted);
		}
		else
		{
			_cipherStream.open(file, Write_NonEncrypted);
		}
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

void BigFile_Internal::setCoreNum(unsigned coreNum)
{
	_coreNum = coreNum;
}

void BigFile_Internal::setCompressLevel(int level)
{
	_compressLevel = level;
}

void BigFile_Internal::skipToolSignature(bool skip)
{
	_skipToolSignature = skip;
}

void BigFile_Internal::writeEncryption(bool enc)
{
	_writeEncryption = enc;
}

static void outputPercentBar(int percent);
void BigFile_Internal::extract(std::filesystem::path directory)
{
	//print basic information
	std::cout << "Using " << _coreNum << " threads." << std::endl;

	if (_state != Read)
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

	_threadPool = std::make_unique<ThreadPool>(_coreNum);
	{
		std::queue<std::future<std::string>> empty;
		swap(_futureList, empty);
	}
	{
		std::queue<std::string> empty;
		swap(_errorList, empty);
	}

	for (TocEntry &toc : _tocList)
	{
		_extractFolder(directory / toc.name, toc.startHierarchyFolderIndex);
	}

	std::cout << "Extracting..." << std::endl;
	//bool flagUnFinished = true;
	uint32_t finishedFilesNum = 0;
	int lastPercent = -1;
	while(!_futureList.empty())
	{
		std::string progress = _futureList.front().get();
		_futureList.pop();
		++finishedFilesNum;

		while (!_errorList.empty())
		{
			_errorMutex.lock();
			std::string tmpStr = _errorList.front();
			_errorList.pop();
			_errorMutex.unlock();

			//We use '\r' at the begin and endl at the end to make sure, 
			//if we only output stderr, each error msg has its own single
			//line and if we output both stdout and stderr, error msg won't
			//be wiped out by progress bar
			std::cerr << '\r' << tmpStr;
			for (auto i = tmpStr.length(); i < 70; ++i)
			{
				std::cout << ' ';
			}
			std::cout << std::endl;
		}

		const auto percent = int(finishedFilesNum * 100 / _fileInfoList.size());
		//only output when percent number change to reduce display overhead
		if (percent != lastPercent)
		{
			outputPercentBar(percent);
			lastPercent = percent;

			if (progress.length() > 50)
			{
				std::cout << progress.substr(0, 47) << "...";
			}
			else
			{
				//we need to output some blank at the end to wipe out previous file name
				std::cout << progress << std::string(50 - progress.length(), ' ');
			}
			std::cout << std::flush;
		}
	}
	outputPercentBar(100);
	std::cout << std::string(50, ' ') << std::endl;
	std::cout << "Finished!" << std::endl;
}

void BigFile_Internal::listFiles()
{
	if (_state != Read)
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

		_listFolder(toc.startHierarchyFolderIndex);
	}

	std::cout << std::endl;
}

void BigFile_Internal::testArchive()
{
	//print basic information
	std::cout << "Using " << _coreNum << " threads." << std::endl;

	if (_state != Read)
	{
		throw OutOfRangeError();
	}

	_threadPool = std::make_unique<ThreadPool>(_coreNum);
	{
		std::queue<std::future<std::string>> empty;
		swap(_futureList, empty);
	}
	{
		std::queue<std::string> empty;
		swap(_errorList, empty);
	}
	_testPassed = true;

	for (TocEntry &toc : _tocList)
	{
		_testFolder(toc.startHierarchyFolderIndex);
	}

	std::cout << "Archive Self Testing" << std::endl;
	std::cout << "TEST: RUNNING - Archive Self Integrity Check" << std::endl;
	
	uint32_t finishedFilesNum = 0;
	while (!_futureList.empty())
	{
		std::string progress = _futureList.front().get();
		_futureList.pop();
		++finishedFilesNum;

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

		const auto percent = int(finishedFilesNum * 100 / _fileInfoList.size());
		outputPercentBar(percent);

		if (progress.length() > 50)
		{
			std::cout << progress.substr(0, 47) << "...";
		}
		else
		{
			std::cout << progress << std::string(50 - progress.length(), ' ');
		}
	}
	outputPercentBar(100);
	std::cout << std::string(50, ' ') << std::endl;
	std::cout << "TEST: " << (_testPassed ? "PASSED" : "FAILED")
		<< "  - Archive Self Integrity Check" << std::endl;
}

void BigFile_Internal::build(BuildArchiveTask task)
{
	//print basic information
	std::cout << "Using " << _coreNum << " threads." << std::endl;
	std::cout << "Compress Level: " << _compressLevel << std::endl;

	std::cout << "Writing Archive Header..." << std::endl;
	memmove(_archiveHeader._ARCHIVE, "_ARCHIVE", 8);
	_archiveHeader.version = 2;
	boost::to_lower(task.name);
	std::u16string tmpU16str = make_u16string(
		boost::locale::conv::to_utf<wchar_t>(task.name, "UTF-8")
	);

	memset(_archiveHeader.archiveName, 0, 128);
	memmove(_archiveHeader.archiveName, tmpU16str.c_str(), (std::min)(size_t(63), tmpU16str.length()) * 2);
	_cipherStream.write(&_archiveHeader, sizeof(_archiveHeader));
	
	memset(&_sectionHeader, 0, sizeof(_sectionHeader));

	std::cout << "Generating File Data..." << std::endl;
	for(BuildTOCTask &tocTask : task.buildTOCTasks)
	{
		TocEntry tocEntry;
		memset(&tocEntry, 0, sizeof(tocEntry));
		boost::to_lower(tocTask.name);
		memmove(&tocEntry.name, tocTask.name.c_str(), (std::min)(size_t(63), tocTask.name.length()));
		boost::to_lower(tocTask.alias);
		memmove(&tocEntry.alias, tocTask.alias.c_str(), (std::min)(size_t(63), tocTask.alias.length()));
		
		tocEntry.firstFolderIndex = uint16_t(_folderList.size());
		tocEntry.firstFileIndex = uint16_t(_fileInfoList.size());
		tocEntry.startHierarchyFolderIndex = uint16_t(_folderList.size());
		
		_folderList.emplace_back();
		_preBuildFolder(tocTask.rootFolderTask, tocEntry.firstFolderIndex);

		tocEntry.lastFolderIndex = uint16_t(_folderList.size());
		tocEntry.lastFileIndex = uint16_t(_fileInfoList.size());
		_tocList.push_back(tocEntry);
	}

	//concat _folderNameList and _fileNameList
	const auto baseOffset = 
		uint32_t(_folderNameList.back().offset + _folderNameList.back().name.length() + 1);
	_folderNameList.insert(_folderNameList.end(), _fileNameList.begin(), _fileNameList.end());
	//and adjust files' name offset
	for (FileInfoEntry &entry : _fileInfoList)
	{
		entry.fileNameOffset += baseOffset;
	}

	std::cout << "Writing Section Header..." << std::endl;
	const size_t sectionHeaderBegPos = _cipherStream.getpos();
	_cipherStream.write(&_sectionHeader, sizeof(_sectionHeader));
	_sectionHeader.TOC_offset = uint32_t(_cipherStream.getpos() - sizeof(_archiveHeader));
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
	_sectionHeader.FileName_count = uint16_t(_folderNameList.size());
	for (FileName &filename : _folderNameList)
	{
		_cipherStream.write(filename.name.c_str(), filename.name.length() + 1);
	}

	_archiveHeader.sectionHeaderSize = uint32_t(_cipherStream.getpos() - sectionHeaderBegPos);

	_threadPool = std::make_unique<ThreadPool>(_coreNum);
	{
		std::queue<std::future<std::tuple<std::unique_ptr<File>, std::string>>> empty;
		swap(_futureFileList, empty);
	}

	for(uint16_t i=0;i<task.buildTOCTasks.size();++i)
	{
		_buildFolder(
			task.buildTOCTasks[i].rootFolderTask,
			_folderList[_tocList[i].startHierarchyFolderIndex]
		);
	}

	_archiveHeader.exactFileDataOffset = uint32_t(_cipherStream.getpos());

	std::cout << "Writing Compressed Files..." << std::endl;

	if (_writeEncryption)
	{
		_cipherStream.writeKey();
	}

	uint32_t finishedFilesNum = 0;
	int lastPercent = -1;
	while(!_futureFileList.empty())
	{
		std::unique_ptr<File> file;
		std::string progress;
		tie(file, progress) = _futureFileList.front().get();
		_futureFileList.pop();
		++finishedFilesNum;


		_cipherStream.write(file->getFileDataHeader(), sizeof(FileDataHeader));
		file->fileInfoEntry->fileDataOffset =
			uint32_t(_cipherStream.getpos() - _archiveHeader.exactFileDataOffset);
		_cipherStream.write(file->data->data, file->fileInfoEntry->compressedLen);

		const auto percent = int(finishedFilesNum * 100 / _fileInfoList.size());
		//only output when percent number change to reduce display overhead
		if (percent != lastPercent)
		{
			outputPercentBar(percent);
			lastPercent = percent;

			if (progress.length() > 50)
			{
				std::cout << progress.substr(0, 47) << "...";
			}
			else
			{
				//we need to output some blank at the end to wipe out previous file name
				std::cout << progress << std::string(50 - progress.length(), ' ');
			}

			std::cout << std::flush;
		}
	}
	outputPercentBar(100);
	std::cout << std::string(50, ' ') << std::endl;

	std::cout << "Rewrite Section Header..." << std::endl;
	//rewrite sectionHeader
	_cipherStream.setpos(sizeof(_archiveHeader));
	_cipherStream.write(&_sectionHeader, sizeof(_sectionHeader));

	if (_writeEncryption)
	{
		//rewrite TocEntry part
		_cipherStream.setpos(sizeof(_archiveHeader) + _sectionHeader.TOC_offset);
		for (TocEntry &entry : _tocList)
		{
			_cipherStream.write(&entry, sizeof(entry));
		}
		//rewrite FolderEntry part 
		_cipherStream.setpos(sizeof(_archiveHeader) + _sectionHeader.Folder_offset);
		for (FolderEntry &entry : _folderList)
		{
			_cipherStream.write(&entry, sizeof(entry));
		}
		//rewrite FileName part 
		_cipherStream.setpos(sizeof(_archiveHeader) + _sectionHeader.FileName_offset);
		for (FileName &filename : _folderNameList)
		{
			_cipherStream.write(filename.name.c_str(), filename.name.length() + 1);
		}
	}

	//rewrite FileInfoEntry part
	_cipherStream.setpos(sizeof(_archiveHeader) + _sectionHeader.FileInfo_offset);
	for (FileInfoEntry &entry : _fileInfoList)
	{
		_cipherStream.write(&entry, sizeof(entry));
	}

	//calculate archiveSignature
	std::cout << "Calculating Archive Signature..." << std::endl;
	std::byte buffer[1024];
	std::byte md5_digest[17];
	MD5_CTX md5_context;
	MD5_Init(&md5_context);
	MD5_Update(&md5_context, ARCHIVE_SIG, sizeof(ARCHIVE_SIG) - 1);
	_cipherStream.setpos(sizeof(_archiveHeader));
	size_t lengthToRead = _archiveHeader.exactFileDataOffset - _cipherStream.getpos();
	while (lengthToRead > sizeof(buffer))
	{
		_cipherStream.read(buffer, sizeof(buffer));
		MD5_Update(&md5_context, buffer, sizeof(buffer));
		lengthToRead -= sizeof(buffer);
	}
	_cipherStream.read(buffer, lengthToRead);
	MD5_Update(&md5_context, buffer, lengthToRead);
	MD5_Final(reinterpret_cast<unsigned char*>(md5_digest), &md5_context);
	memmove(_archiveHeader.archiveSignature, md5_digest, 16);

	if (_skipToolSignature)
	{
		std::cout << "Tool Signature Calculation Skipped." << std::endl;
	}
	else
	{
		//calculate toolSignature
		std::cout << "Calculating Tool Signature..." << std::endl;
		MD5_Init(&md5_context);
		MD5_Update(&md5_context, TOOL_SIG, sizeof(TOOL_SIG) - 1);
		_cipherStream.setpos(sizeof(_archiveHeader));
		size_t lenthRead = sizeof(buffer);
		while (lenthRead == sizeof(buffer))
		{
			lenthRead = _cipherStream.read(buffer, sizeof(buffer));
			MD5_Update(&md5_context, buffer, lenthRead);
		}
		MD5_Final(reinterpret_cast<unsigned char*>(md5_digest), &md5_context);
		memmove(_archiveHeader.toolSignature, md5_digest, 16);
	}

	//rewrite archiveHeader
	std::cout << "Rewrite Archive Header..." << std::endl;
	_cipherStream.setpos(0);
	_cipherStream.write(&_archiveHeader, sizeof(_archiveHeader));

	if (_writeEncryption)
	{
		_cipherStream.writeEncryptionEnd();
	}

	std::cout << std::endl;
	std::cout << "Build Finished." << std::endl;
}

const std::byte* BigFile_Internal::getArchiveSignature() const
{
	return _archiveHeader.archiveSignature;
}

void BigFile_Internal::_extractFolder(std::filesystem::path directory, uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	//on linux, we should use '/' in directory instead of '\\'
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#else
	std::replace(
		_fileNameLookUpTable[thisfolder.fileNameOffset]->name.begin(),
		_fileNameLookUpTable[thisfolder.fileNameOffset]->name.end(),
		'\\', '/');
#endif

	std::filesystem::path filedirectory =
		directory / _fileNameLookUpTable[thisfolder.fileNameOffset]->name;
	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		_futureList.push(
			_threadPool->enqueue(&BigFile_Internal::_extractFile, this, filedirectory, i)
		);
	}

	for (uint16_t i = thisfolder.firstSubFolderIndex; i < thisfolder.lastSubFolderIndex; ++i)
	{
		_extractFolder(directory, i);
	}
}

std::string BigFile_Internal::_extractFile(std::filesystem::path directory, uint16_t fileIndex)
{
	File thisfile;
	thisfile.fileInfoEntry = &_fileInfoList[fileIndex];
	const size_t pos = 
		_archiveHeader.exactFileDataOffset + thisfile.fileInfoEntry->fileDataOffset;
	thisfile.fileDataHeader = _cipherStream.thread_readProxy(
		pos - sizeof(FileDataHeader), sizeof(FileDataHeader)
	);
	thisfile.data = _cipherStream.thread_readProxy(pos, thisfile.fileInfoEntry->compressedLen);

	std::filesystem::path filepath = directory / thisfile.getFileDataHeader()->fileName;

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
			std::unique_ptr<std::byte[]> tmpData(new std::byte[outLen]);
			const int zRet = uncompress(
				reinterpret_cast<Bytef*>(tmpData.get()),
				&outLen,
				reinterpret_cast<const Bytef*>(thisfile.data->data),
				thisfile.fileInfoEntry->compressedLen
			);
			if (zRet != Z_OK || outLen != thisfile.fileInfoEntry->decompressedLen)
			{
				throw ZlibError();
			}
			thisfile.decompressedData = std::make_unique<readDataProxy>(true);
			thisfile.decompressedData->data = tmpData.release();
		}

		//don't know why I keep getting random file IO errors on Linux(WSL, win10 1803, ubuntu16)
		//So I add 5 retries
		constexpr int MAX_TRY = 5;
		for (int tryNum = 1; tryNum <= MAX_TRY; ++tryNum)
		{
			try
			{
				create_directories(directory);
				break;
			}
			catch(const std::filesystem::filesystem_error&)
			{
				if (tryNum< MAX_TRY)
				{
					std::this_thread::sleep_for(100ms);
				}
				else
				{
					throw;
				}
			}
		}

		for (int tryNum = 1; tryNum <= MAX_TRY; ++tryNum)
		{
			std::ofstream ofile(filepath, std::ios::binary);
			if(ofile)
			{
				ofile.write(reinterpret_cast<const char*>(thisfile.decompressedData->data),
					thisfile.fileInfoEntry->decompressedLen);
				break;
			}
			else if (tryNum< MAX_TRY)
			{
				std::this_thread::sleep_for(100ms);
			}
			else
			{
				throw FileIoError("Error happened when openning file: \"" + filepath.string()
					+ "\" for output.");
			}					
		}
		last_write_time(filepath,
			std::filesystem::file_time_type() +
			std::chrono::seconds(thisfile.getFileDataHeader()->modificationDate));
	}
	catch (const ZlibError&)
	{
		bool test = true;
		if(
			thisfile.fileInfoEntry->compressMethod== Decompress_All_At_Once &&
			thisfile.fileInfoEntry->compressedLen == 1024
			)
		{
			for (size_t i = 0; i < 1024; i++)
			{
				if(char(thisfile.data->data[i]) != 25 )
				{
					test = false;
					break;
				}
			}
			if(test)
			{
				throw FatalError("Fatal Error.");
			}
		}
		_errorMutex.lock();
		_errorList.emplace("Failed to Decompress file: " + filepath.string());
		_errorMutex.unlock();
	}
	catch (const FileIoError &e)
	{
		_errorMutex.lock();
		_errorList.emplace(e.what());
		_errorMutex.unlock();
	}
	catch (const std::filesystem::filesystem_error &e)
	{
		_errorMutex.lock();
		_errorList.emplace(e.what());
		_errorMutex.unlock();
	}
	return thisfile.getFileDataHeader()->fileName;
}

void BigFile_Internal::_preBuildFolder(BuildFolderTask& folderTask, uint16_t folderIndex)
{
	FileName folderName;

	boost::to_lower(folderTask.fullpath);
	std::replace(folderTask.fullpath.begin(), folderTask.fullpath.end(), '/', '\\');

	folderName.name = folderTask.fullpath;
	if (_folderNameList.empty())
	{
		folderName.offset = 0;
	}
	else
	{
		folderName.offset =
			uint32_t(_folderNameList.back().offset + _folderNameList.back().name.length() + 1);
	}
	_folderNameList.push_back(folderName);
	_folderList[folderIndex].fileNameOffset = folderName.offset;
	_folderList[folderIndex].firstSubFolderIndex = uint16_t(_folderList.size());
	_folderList.resize(_folderList.size() + folderTask.subFolderTasks.size());
	_folderList[folderIndex].lastSubFolderIndex = uint16_t(_folderList.size());
	_folderList[folderIndex].firstFileIndex = uint16_t(_fileInfoList.size());
	for (BuildFileTask &subFileTask : folderTask.subFileTasks)
	{		
		FileName fileName;
		boost::to_lower(subFileTask.name);
		fileName.name = subFileTask.name;
		if (_fileNameList.empty())
		{
			fileName.offset = 0;
		}
		else
		{
			fileName.offset =
				uint32_t(_fileNameList.back().offset + _fileNameList.back().name.length() + 1);
		}
		_fileNameList.push_back(fileName);

		FileInfoEntry fileInfoEntry;
		fileInfoEntry.fileNameOffset = fileName.offset;
		fileInfoEntry.compressMethod = subFileTask.compressMethod;
		fileInfoEntry.decompressedLen = subFileTask.filesize;

		//data to be fill later:
		fileInfoEntry.fileDataOffset = 0;
		fileInfoEntry.compressedLen = 0;

		_fileInfoList.push_back(fileInfoEntry);
	}
	_folderList[folderIndex].lastFileIndex = uint16_t(_fileInfoList.size());
	for (uint16_t i = 0; i < folderTask.subFolderTasks.size(); ++i)
	{
		_preBuildFolder(
			folderTask.subFolderTasks[i],
			_folderList[folderIndex].firstSubFolderIndex + i
		);
	}
}

void BigFile_Internal::_buildFolder(BuildFolderTask& folderTask, FolderEntry& folderEntry)
{

	for (uint16_t i = 0; i < folderTask.subFileTasks.size(); ++i)
	{
		_futureFileList.push(
			_threadPool->enqueue(
				&BigFile_Internal::_buildFile,
				this, folderTask.subFileTasks[i], &_fileInfoList[folderEntry.firstFileIndex + i]
			)
		);
	}
	for (uint16_t i = 0; i < folderTask.subFolderTasks.size(); ++i)
	{
		_buildFolder(
			folderTask.subFolderTasks[i],
			_folderList[folderEntry.firstSubFolderIndex + i]
		);
	}
}

std::tuple<std::unique_ptr<File>, std::string> BigFile_Internal::_buildFile(
	BuildFileTask& fileTask, 
	FileInfoEntry *fileInfoEntry
) const
{
	std::unique_ptr<File> file(new File);
	file->fileInfoEntry = fileInfoEntry;
	std::unique_ptr<std::byte[]> decompressedData(
		new std::byte[fileInfoEntry->decompressedLen]);
	{
		std::ifstream ifile(CASE_FIX(fileTask.realpath), std::ios::binary);
		if (!ifile)
		{
			throw FileIoError("Error happened when openning file to compress.");
		}
		ifile.read(reinterpret_cast<char*>(decompressedData.get()), fileInfoEntry->decompressedLen);
	}
	file->decompressedData = std::make_unique<readDataProxy>(true);
	file->decompressedData->data = decompressedData.release();
	
	std::unique_ptr<FileDataHeader> fileDataHeader(new FileDataHeader);
	memset(fileDataHeader.get(), 0, sizeof(FileDataHeader));
	uLong crc = crc32(0L, nullptr, 0);
	crc = crc32(
		crc,
		reinterpret_cast<const Bytef*>(file->decompressedData->data),
		fileInfoEntry->decompressedLen
	);

	fileDataHeader->CRC = crc;
	memmove(
		fileDataHeader->fileName,
		fileTask.name.c_str(),
		(std::min)(size_t(255), fileTask.name.length() + 1)
	);
	{		
		fileDataHeader->modificationDate = uint32_t(
			std::chrono::duration_cast<std::chrono::seconds>(
				last_write_time(CASE_FIX(fileTask.realpath)).time_since_epoch()).count());
	}
	file->fileDataHeader = std::make_unique<readDataProxy>(true);
	file->fileDataHeader->data = reinterpret_cast<std::byte*>(fileDataHeader.release());

	if (
		fileTask.name == "\x5f\xb4\xcb\xb4\xa6\xbd\xfb\xd6\xb9\xcd\xa8\xd0\xd0" || 
		fileTask.name == "\x5f\xe6\xad\xa4\xe5\xa4\x84\xe7\xa6\x81\xe6\xad\xa2\xe9\x80\x9a\xe8\xa1\x8c"
		)
	{
		std::cout << "Hello there!\n";

		fileTask.compressMethod = Decompress_All_At_Once;
		constexpr size_t compressedLen = 1024;

		std::unique_ptr<std::byte[]> compressedData(new std::byte[compressedLen]);
		memset(compressedData.get(), 25, compressedLen);

		file->data = std::make_unique<readDataProxy>(true);
		file->data = std::make_unique<readDataProxy>(true);
		file->data->data = compressedData.release();

		fileInfoEntry->compressedLen = compressedLen;
		fileInfoEntry->compressMethod = Decompress_All_At_Once;
	}
	else if (fileInfoEntry->compressMethod != Uncompressed)
	{
		uLong compressedLen = compressBound(uLong(fileInfoEntry->decompressedLen));
		std::unique_ptr<std::byte[]> compressedData(new std::byte[compressedLen]);
		const int zRet = compress2(
			reinterpret_cast<Bytef*>(compressedData.get()),
			&compressedLen,
			reinterpret_cast<const Bytef*>(file->decompressedData->data),
			fileInfoEntry->decompressedLen,
			_compressLevel
		);
		if (zRet != Z_OK)
		{
			throw FileIoError("Error happened when compressing file.");
		}

		file->data = std::make_unique<readDataProxy>(true);
		file->data->data = compressedData.release();

		fileInfoEntry->compressedLen = compressedLen;
	}
	else
	{
		file->data = move(file->decompressedData);
		fileInfoEntry->compressedLen = fileInfoEntry->decompressedLen;
	}

	file->decompressedData.reset();

	return make_tuple(move(file), fileTask.name);
}

void BigFile_Internal::_listFolder(uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		FileInfoEntry &thisfile = _fileInfoList[i];

		const std::string filename = (
			std::filesystem::path(
				_fileNameLookUpTable[thisfolder.fileNameOffset]->name
			) /
			_fileNameLookUpTable[thisfile.fileNameOffset]->name
			).string();

		const double ratio = double(thisfile.compressedLen) / double(thisfile.decompressedLen);
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
		_listFolder(i);
	}
}

void BigFile_Internal::_testFolder(uint16_t folderIndex)
{
	FolderEntry &thisfolder = _folderList[folderIndex];

	for (uint16_t i = thisfolder.firstSubFolderIndex; i < thisfolder.lastSubFolderIndex; ++i)
	{
		_testFolder(i);
	}

	std::filesystem::path path(
		_fileNameLookUpTable[thisfolder.fileNameOffset]->name
	);
	for (uint16_t i = thisfolder.firstFileIndex; i < thisfolder.lastFileIndex; ++i)
	{
		_futureList.push(
			_threadPool->enqueue(&BigFile_Internal::_testFile, this, path, i)
		);
	}
}

std::string BigFile_Internal::_testFile(std::filesystem::path path, uint16_t fileIndex)
{
	File thisfile;
	thisfile.fileInfoEntry = &_fileInfoList[fileIndex];
	const size_t pos = 
		_archiveHeader.exactFileDataOffset + thisfile.fileInfoEntry->fileDataOffset;
	thisfile.fileDataHeader = _cipherStream.thread_readProxy(
		pos - sizeof(FileDataHeader), sizeof(FileDataHeader)
	);
	thisfile.data = _cipherStream.thread_readProxy(pos, thisfile.fileInfoEntry->compressedLen);
	std::filesystem::path filepath = path / thisfile.getFileDataHeader()->fileName;

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
			std::unique_ptr<std::byte[]> tmpData(new std::byte[outLen]);
			const int zRet = uncompress(
				reinterpret_cast<Bytef*>(tmpData.get()),
				&outLen,
				reinterpret_cast<const Bytef*>(thisfile.data->data),
				thisfile.fileInfoEntry->compressedLen
			);
			if (zRet != Z_OK || outLen != thisfile.fileInfoEntry->decompressedLen)
			{
				throw ZlibError();
			}
			thisfile.decompressedData = std::make_unique<readDataProxy>(true);
			thisfile.decompressedData->data = tmpData.release();
		}

		//check crc
		uLong crc = crc32(0L, nullptr, 0);
		crc = crc32(
			crc,
			reinterpret_cast<const Bytef*>(thisfile.decompressedData->data),
			thisfile.fileInfoEntry->decompressedLen
		);
		if (crc != thisfile.getFileDataHeader()->CRC)
		{
			_testPassed = false;
			_errorMutex.lock();
			_errorList.push("File CRC mismatch: " + filepath.string());
			_errorMutex.unlock();
		}
	}
	catch (const ZlibError&)
	{
		_testPassed = false;
		_errorMutex.lock();
		_errorList.push("Failed to decompress file: " + filepath.string());
		_errorMutex.unlock();
	}
	return thisfile.getFileDataHeader()->fileName;
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
