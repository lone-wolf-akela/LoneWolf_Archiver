#pragma once
#include "stdafx.h"

class FileStream
{
public:
	virtual ~FileStream() = default;
	virtual void read(void* dst, size_t length) = 0;
	virtual void write(void* src, size_t length) = 0;
	virtual void setpos(size_t pos) = 0;
};