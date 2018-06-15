#pragma once
#include "stdafx.h"

namespace ext
{
	struct read
	{
		read(void *dst, std::streamsize len):
		dst(dst),len(len)
		{}
		void *dst;
		std::streamsize len;
	};

	inline std::istream& operator|
		(
			std::istream& s,
			read&& r
		)
	{
		return s.read(static_cast<char*>(r.dst), r.len);
	}

	struct write
	{
		write(const void *src, std::streamsize len) :
			src(src), len(len)
		{}
		const void *src;
		std::streamsize len;
	};

	inline std::ostream& operator|
		(
			std::ostream& s,
			write&& w
		)
	{
		return s.write(static_cast<const char*>(w.src), w.len);
	}
}

