#pragma once
#include <cstddef>

#include <vector>

#define ZLIB_CONST // this make z_stream.next_in const
#include <zlib.h>

#include "../ThreadPool/ThreadPool.h"
#include "../exceptions/exceptions.h"

namespace compressor
{
	std::future<std::vector<std::byte>> compress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level);
	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize);
}
