#pragma once
#include "stdafx.h"

#include "filestream.h"

class MemMapFileStream : public FileStream
{
public:
	MemMapFileStream(void) = default;
	explicit MemMapFileStream(boost::filesystem::path const &file)
	{
		open(file);
	}

	void open(boost::filesystem::path const &file);
	void close(void);

	size_t read(void *dst, size_t length) override;
	std::unique_ptr<readDataProxy> readProxy(size_t length) override;
	void write(const void *src, size_t length) override;

	void thread_read(size_t pos, void *dst, size_t length) override;
	std::unique_ptr<readDataProxy> thread_readProxy(size_t pos, size_t length) override;

	void setpos(size_t pos) override;
	size_t getpos(void) override;
	void movepos(signed_size_t diff) override;

	const std::byte *getReadptr(void) const;
	uintmax_t getFileSize(void) const;
private:
	boost::iostreams::mapped_file_source _filesrc;
	uintmax_t _filesize = 0;
	const std::byte *_readptr = nullptr;
};