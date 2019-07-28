#include <cstring>

#include "memmapfilestream.h"

namespace stream
{
	void MemMapFileStream::open(const std::filesystem::path& file)
	{
		_filesize = file_size(file);
		_filesrc.open(file.string());
		if (!_filesrc)
		{
			throw FileIoError("Error happened when opening file: " +
				file.string() + " for memory mapping.");
		}
		_pos = 0;
	}

	void MemMapFileStream::close(void)
	{
		_filesrc.close();
	}

	size_t MemMapFileStream::read(void* dst, size_t length)
	{
		const size_t l = read(_pos, dst, length);
		movepos(l);
		return l;
	}
	std::tuple<OptionalOwnerBuffer, size_t>
		MemMapFileStream::optionalOwnerRead(size_t length)
	{
		auto r = optionalOwnerRead(_pos, length);
		movepos(std::get<1>(r));
		return r;
	}

	void MemMapFileStream::write(const void* src, size_t length)
	{
		throw NotImplementedError();
	}

	size_t MemMapFileStream::read(size_t pos, void* dst, size_t length)
	{
		//brackets around std::min is required to prevent some problems
		const size_t lengthToRead = (std::min)(size_t(_filesize - pos), length);
		memmove(dst, _filesrc.data() + pos, lengthToRead);
		return lengthToRead;
	}

	std::tuple<OptionalOwnerBuffer, size_t>
		MemMapFileStream::optionalOwnerRead(size_t pos, size_t length)
	{
		//brackets around std::min is required to prevent some problems
		const size_t lengthToRead = (std::min)(size_t(_filesize - pos), length);
		return { OptionalOwnerBuffer(
			reinterpret_cast<const std::byte*>(_filesrc.data()) + pos),
			lengthToRead };
	}

	void MemMapFileStream::setpos(size_t pos)
	{
		if (pos > _filesize)
		{
			throw OutOfRangeError();
		}
		_pos = pos;
	}

	size_t MemMapFileStream::getpos(void)
	{
		return _pos;
	}

	void MemMapFileStream::movepos(ptrdiff_t diff)
	{
		if ((diff < 0 && getpos() < size_t(-diff)) ||
			(diff > 0 && getpos() + diff > _filesize))
		{
			throw OutOfRangeError();
		}
		_pos += diff;
	}

	const std::byte* MemMapFileStream::getReadptr() const
	{
		return reinterpret_cast<const std::byte*>(_filesrc.data()) + _pos;
	}

	uintmax_t MemMapFileStream::getFileSize() const
	{
		return _filesize;
	}
}
