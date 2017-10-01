#include "stdafx.h"

#include "memmapfilestream.h"

void MemMapFileStream::open(boost::filesystem::path file)
{
	_filesize = file_size(file);

	boost::iostreams::mapped_file_params params;
	params.path = file.generic_string();
	params.flags = boost::iostreams::mapped_file::mapmode::readonly;
	_filesrc.open(params);

	if (!_filesrc.is_open())
	{
		throw FileIoError("Error happened when opening file for memory mapping.");
	}

	_readptr = _filesrc.data();
}

void MemMapFileStream::close(void)
{
	_filesrc.close();
}

void MemMapFileStream::read(void * dst, size_t length)
{
	memcpy(dst, _readptr, length);
	movepos(length);
}

std::unique_ptr<readDataProxy> MemMapFileStream::readProxy(size_t length)
{
	std::unique_ptr<readDataProxy> proxy(new readDataProxy(false));
	proxy->data = _readptr;
	movepos(length);
	return proxy;
}

void MemMapFileStream::write(const void * src, size_t length)
{
	throw NotImplementedError();
}

void MemMapFileStream::thread_read(size_t pos, void* dst, size_t length)
{
	memcpy(dst, _filesrc.data() + pos, length);
}

std::unique_ptr<readDataProxy> MemMapFileStream::thread_readProxy(size_t pos, size_t length)
{
	std::unique_ptr<readDataProxy> proxy(new readDataProxy(false));
	proxy->data = _filesrc.data() + pos;
	return proxy;
}

void MemMapFileStream::setpos(size_t pos)
{
	if (pos > _filesize)
	{
		throw OutOfRangeError();
	}
	_readptr = _filesrc.data() + pos;
}

size_t MemMapFileStream::getpos(void)
{
	return _readptr - _filesrc.data();
}

void MemMapFileStream::movepos(signed_size_t diff)
{
	if (
		(diff < 0 && _readptr - _filesrc.data() < -diff) ||
		(diff > 0 && uintmax_t(_readptr - _filesrc.data()) + diff > _filesize)
		)
	{
		throw OutOfRangeError();
	}
	_readptr += diff;
}

const char* MemMapFileStream::getReadptr() const
{
	return _readptr;
}

uintmax_t MemMapFileStream::getFileSize() const
{
	return _filesize;
}
