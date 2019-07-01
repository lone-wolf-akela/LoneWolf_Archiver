﻿#pragma once
#include <cstddef>

#include <vector>

#include "../ThreadPool/ThreadPool.h"

namespace compressor
{
	std::future<std::vector<std::byte>> compress(
		ThreadPool& pool, std::byte* data, size_t inputsize, int level);
	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, std::byte* data, size_t inputsize, size_t outputsize);
}