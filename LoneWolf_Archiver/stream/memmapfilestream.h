#pragma once
#include <filesystem>

#include <boost/iostreams/device/mapped_file.hpp>

#include "../exceptions/exceptions.h"
#include "filestream.h"

namespace stream
{
	class MemMapFileStream : public FileStream
	{
	public:
		MemMapFileStream() = default;
		explicit MemMapFileStream(std::filesystem::path const& file)
		{
			open(file);
		}

		void open(const std::filesystem::path& file);
		void close();

		size_t read(void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t length) override;
		void write(const void* src, size_t length) override;

		//these two do not move the read ptr
		size_t read(size_t pos, void* dst, size_t length) override;
		std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t pos, size_t length) override;

		void setpos(size_t pos) override;
		size_t getpos() override;
		void movepos(ptrdiff_t diff) override;

		[[nodiscard]] const std::byte* getReadptr() const;
		[[nodiscard]] uintmax_t getFileSize() const;
	private:
		boost::iostreams::mapped_file_source _filesrc;
		uintmax_t _filesize = 0;
		size_t _pos = 0;
	};
}
