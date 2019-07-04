#include <cassert>
#include <array>

#include "compressor.h"

/*
 *Part of code in this file is copied from pigz(https://zlib.net/pigz/)
 *The license from pigz.c is copied here:
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty.  In no event will the author be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
*/

#define BASE 65521U     // largest prime smaller than 65536
#define LOW16 0xffff    // mask lower 16 bits

namespace
{
	constexpr size_t partinsize = 8 * 1024 * 1024; // 4MB per part
	constexpr size_t partboundsize = 12 * 1024 * 1024; // file smaller than this won't be partitioned
	constexpr int windowBits = 15; // larger = better compression & more memory use
	constexpr int memLevel = 9; // similar to windowBits, larger = better compression & memory use

	///\note this function is copied from pigz
	uint32_t adler32_comb(uint32_t adler1, uint32_t adler2, size_t len2)
	{
		unsigned long sum1;
		unsigned long sum2;
		unsigned rem;
		// the derivation of this formula is left as an exercise for the reader
		rem = (unsigned)(len2 % BASE);
		sum1 = adler1 & LOW16;
		sum2 = (rem * sum1) % BASE;
		sum1 += (adler2 & LOW16) + BASE - 1;
		sum2 += ((adler1 >> 16) & LOW16) + ((adler2 >> 16) & LOW16) + BASE - rem;
		if (sum1 >= BASE) sum1 -= BASE;
		if (sum1 >= BASE) sum1 -= BASE;
		if (sum2 >= (BASE << 1)) sum2 -= (BASE << 1);
		if (sum2 >= BASE) sum2 -= BASE;
		return sum1 | (sum2 << 16);
	}
	///\note this function is copied from pigz
	std::array<std::byte, 2> get_header(int level)
	{
		std::array<std::byte, 2> head_BE;
		uint16_t head;
		head = (0x78 << 8) +        // deflate, 32K window
			(level >= 9 ? 3 << 6 :
				level == 1 ? 0 << 6 :
				level >= 6 || level == Z_DEFAULT_COMPRESSION ? 1 << 6 :
				2 << 6);            // optional compression level clue
		head += 31 - (head % 31);   // make it a multiple of 31
		// zlib format uses big-endian order
		head_BE[0] = std::byte((head >> 8) & 0xff);
		head_BE[1] = std::byte(head & 0xff);
		return head_BE;
	}

	/// \return <output_length, checksum>
	std::tuple<uLong, uint32_t> compress_worker_part(
		const std::byte* in,
		std::byte* out,
		size_t insize,
		size_t outsize,
		int level,
		bool lastpart)
	{
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		// don't really understand why strm.next_in is not a const...
		strm.next_in = reinterpret_cast<const Bytef*>(in);
		strm.avail_in = uInt(insize);
		strm.next_out = reinterpret_cast<Bytef*>(out);
		strm.avail_out = uInt(outsize);
		assert(Z_OK == deflateInit2(
			&strm, level, Z_DEFLATED, -windowBits, memLevel, Z_DEFAULT_STRATEGY));
		if (lastpart)
		{
			assert(Z_STREAM_END == deflate(&strm, Z_FINISH));
		}
		else
		{
			assert(Z_OK == deflate(&strm, Z_FULL_FLUSH));
		}
		uLong outlen = strm.total_out;
		uint32_t check = adler32(0, nullptr, 0);
		check = adler32(check, reinterpret_cast<const Bytef*>(in), uInt(insize));
		// deflateEnd is supposed to return Z_OK
		// but its seems to return a strange value
		// so I don't check it
		// assert(Z_OK == deflateEnd(&strm));
		deflateEnd(&strm);
		return std::make_tuple(outlen, check);
	}
	std::vector<std::byte> compress_worker_nonpart(
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
		r.shrink_to_fit();
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

#define CEIL_DIV(x, y)  ((x)/(y) + ((x) % (y) != 0))
namespace compressor
{
	std::future<std::vector<std::byte>> compress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level)
	{
		if (inputsize >= partboundsize)
		{
			const size_t partnum = CEIL_DIV(inputsize, partinsize);
			const uLong partoutsize = compressBound(partinsize);
			std::vector<std::byte> compressed(partnum * partoutsize);
			std::vector<std::future<std::tuple<uLong, uint32_t>>> futures;
			for (size_t i = 0; i < partnum; i++)
			{
				futures.push_back(pool.enqueue(compress_worker_part,
					data + i * partinsize,
					compressed.data() + i * partoutsize,
					partnum - 1 == i ? inputsize - i * partinsize : partinsize,
					partoutsize,
					level,
					partnum - 1 == i));
			}
			return std::async(std::launch::deferred,
				[futures = std::move(futures),
				compressed = std::move(compressed),
				level,
				partoutsize,
				inputsize]() mutable
			{
				uint32_t check_comb = adler32(0, nullptr, 0);
				auto header = get_header(level);
				// reserve bytes for header
				std::byte* p = compressed.data() + header.size();
				// merge data
				for (int i = 0; i < futures.size(); i++)
				{
					auto [outlen, check] = futures[i].get();
					memmove(p, compressed.data() + i * partoutsize, outlen);
					p += outlen;
					check_comb = adler32_comb(check_comb,
						check,
						futures.size() - 1 == i ? inputsize - i * partinsize : partinsize);
				}
				// write header
				memmove(compressed.data(), header.data(), header.size());
				// write trailer
				*p++ = std::byte((check_comb >> 24) & 0xff);
				*p++ = std::byte((check_comb >> 16) & 0xff);
				*p++ = std::byte((check_comb >> 8) & 0xff);
				*p++ = std::byte(check_comb & 0xff);
				compressed.resize(p - compressed.data());
				compressed.shrink_to_fit();
				return compressed;
			});
		}
		else
		{
			return pool.enqueue(compress_worker_nonpart, data, inputsize, level);
		}
	}

	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize)
	{
		return pool.enqueue(uncompress_worker, data, inputsize, outputsize);
	}
}
