#pragma once
#include "stdafx.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define CASE_FIX(x) (x)
#else
std::filesystem::path linuxPathCaseFix(std::filesystem::path& path);
#define CASE_FIX(x) (linuxPathCaseFix(x))
#endif

std::u16string make_u16string(const std::wstring& ws);