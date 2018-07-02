#pragma once
#include "stdafx.h"

namespace ext
{
	inline auto read = boost::hof::pipable(
		[](std::istream& s, void *dst, std::streamsize len)
		-> std::istream&
	{
		return s.read(static_cast<char*>(dst), len);
	});

	inline auto write = boost::hof::pipable(
		[](std::ostream& s, const void *src, std::streamsize len)
		->std::ostream&
	{
		return s.write(static_cast<const char*>(src), len);
	});
}

