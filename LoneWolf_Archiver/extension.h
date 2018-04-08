#pragma once
#include "stdafx.h"

namespace ext
{
	inline std::istream& read(std::istream& s,void *dst, std::streamsize len)
	{
		return s.read(static_cast<char*>(dst), len);
	}

	inline std::ostream& write(std::ostream& s,const void* src,std::streamsize len)
	{
		return s.write(static_cast<const char*>(src), len);
	}
}

