#include "stdafx.h"

#include "HWRM_BigFile.h"

void BigFile::open(std::experimental::filesystem::path file)
{
	_internal.open(file, read);
}

void BigFile::close()
{
	_internal.close();
}

void BigFile::extract(std::experimental::filesystem::path directory)
{
	_internal.extract(directory);
}
