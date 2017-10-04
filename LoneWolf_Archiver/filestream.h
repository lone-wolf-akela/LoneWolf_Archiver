#pragma once
#include "stdafx.h"

typedef std::make_signed<size_t>::type signed_size_t;

class readDataProxy
{
public:
	explicit readDataProxy(bool needDelete) :_needDelete(needDelete)
	{}
	~readDataProxy(void)
	{
		if (_needDelete)
		{
			delete[] data;
		}
	}

	readDataProxy(const readDataProxy&) = delete; // non construction-copyable
	readDataProxy& operator=(const readDataProxy&) = delete; // non copyable

	const char *data = nullptr;
private:
	bool _needDelete;
};

class FileStream
{
public:
	virtual ~FileStream() = default;

	virtual size_t read(void *dst, size_t length) = 0;
	virtual std::unique_ptr<readDataProxy> readProxy(size_t length) = 0;
	virtual void write(const void *src, size_t length) = 0;

	virtual void thread_read(size_t pos, void *dst, size_t length) = 0;
	virtual std::unique_ptr<readDataProxy> thread_readProxy(size_t pos, size_t length) = 0;

	virtual void setpos(size_t pos) = 0;
	virtual size_t getpos(void) = 0;
	virtual void movepos(signed_size_t diff) = 0;
};