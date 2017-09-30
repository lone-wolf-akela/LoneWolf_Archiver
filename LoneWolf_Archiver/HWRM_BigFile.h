#pragma once
#include "stdafx.h"
#include "HWRM_BigFile_Internal.h"

class BigFile
{
public:
	BigFile() = default;
	BigFile(std::experimental::filesystem::path file)
	{
		open(file);
	}

	void open(std::experimental::filesystem::path file);
	void close(void);

	void extract(std::experimental::filesystem::path directory);

	~BigFile(void)
	{
		close();
	}
private:
	BigFile_Internal _internal;
};
