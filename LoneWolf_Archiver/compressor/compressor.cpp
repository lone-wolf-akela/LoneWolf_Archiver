#include <cassert>

#include "compressor.h"

namespace
{
	std::vector<std::byte> compress_worker(
		const std::byte* data, size_t inputsize, int level)
	{		
		uLongf compressed_size = compressBound(uLong(inputsize));
		std::vector<std::byte> r(compressed_size);
		assert(Z_OK == compress2(
			reinterpret_cast<Bytef*>(r.data()),
			&compressed_size,
			reinterpret_cast<const Bytef*>(data),
			uLong(inputsize),
			level));
		r.resize(compressed_size);
		return r;
	}

	std::vector<std::byte> uncompress_worker(
		const std::byte* data, size_t inputsize, size_t outputsize)
	{
		std::vector<std::byte> r(outputsize);
		uLongf ul_outputsize = uLongf(outputsize);
		assert(Z_OK == uncompress(
			reinterpret_cast<Bytef*>(r.data()),
			&ul_outputsize,
			reinterpret_cast<const Bytef*>(data),
			uLong(inputsize) && ul_outputsize == outputsize));
		return r;
	}
}

namespace compressor
{
	std::future<std::vector<std::byte>> compress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level)
	{
		return pool.enqueue(compress_worker, data, inputsize, level);
	}

	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize)
	{
		return pool.enqueue(uncompress_worker, data, inputsize, outputsize);
	}
}
