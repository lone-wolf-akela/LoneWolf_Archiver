#include "stdafx.h"

#include "memmapfilestream.h"

void MemMapFileStream::open(boost::filesystem::path const &file)
{
	_filesize = file_size(file);

	boost::iostreams::mapped_file_params params;
	params.path = file.string();
	params.flags = boost::iostreams::mapped_file::mapmode::readonly;
	_filesrc.open(params);

	if (!_filesrc)
	{
		throw FileIoError("Error happened when opening file for memory mapping.");
	}

	_readptr = reinterpret_cast<const std::byte*>(_filesrc.data());
}

void MemMapFileStream::close(void)
{
	_filesrc.close();
}

size_t MemMapFileStream::read(void * dst, size_t length)
{
	//brackets around std::min is required to prevent some problems
	const size_t lengthToRead = (std::min)(size_t(_filesize - getpos()), length);
	memcpy(dst, _readptr, lengthToRead);
	movepos(lengthToRead);
	return lengthToRead;	
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
	proxy->data = reinterpret_cast<const std::byte*>(_filesrc.data()) + pos;
	return proxy;
}

void MemMapFileStream::setpos(size_t pos)
{
	if (pos > _filesize)
	{
		throw OutOfRangeError();
	}
	_readptr = reinterpret_cast<const std::byte*>(_filesrc.data()) + pos;
}

size_t MemMapFileStream::getpos(void)
{
	return _readptr - reinterpret_cast<const std::byte*>(_filesrc.data());
}

void MemMapFileStream::movepos(signed_size_t diff)
{
	if (
		(diff < 0 && getpos() < size_t(-diff)) ||
		(diff > 0 && getpos() + diff > _filesize)
		)
	{
		throw OutOfRangeError();
	}
	_readptr += diff;
}

const std::byte* MemMapFileStream::getReadptr() const
{
	return _readptr;
}

uintmax_t MemMapFileStream::getFileSize() const
{
	return _filesize;
}
