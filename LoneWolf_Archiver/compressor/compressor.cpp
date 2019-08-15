#include <array>
#include <algorithm>
#include <numeric>

#include "../helper/helper.h"
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


namespace
{
	void checkErr(bool b)
	{
		if (!b) throw ZlibError();
	}
	template <typename T>
	T* checkNull(T* p)
	{
		if (nullptr == p) throw ZlibError();
		return p;
	}

	constexpr size_t partinsize = 8 * 1024 * 1024; // 4MB per part
	constexpr size_t partboundsize = 12 * 1024 * 1024; // file smaller than this won't be partitioned
	constexpr int windowBits = 15; // larger = better compression & more memory use
	constexpr int memLevel = 9; // similar to windowBits, larger = better compression & memory use

	///\note this function is copied from pigz
	uint32_t adler32_comb(uint32_t adler1, uint32_t adler2, size_t len2)
	{
		constexpr auto BASE = 65521U;	// largest prime smaller than 65536
		constexpr auto LOW16 = 0xffff;	// mask lower 16 bits

		// the derivation of this formula is left as an exercise for the reader
		const auto rem = unsigned(len2 % BASE);
		unsigned long sum1 = adler1 & LOW16;
		unsigned long sum2 = (rem * sum1) % BASE;
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
		uint16_t head = (0x78 << 8) +        // deflate, 32K window
			(level >= 9 ? 3 << 6 :
				level == 1 ? 0 << 6 :
				level >= 6 || level == Z_DEFAULT_COMPRESSION ? 1 << 6 :
				2 << 6);            // optional compression level clue
		head += 31 - (head % 31);   // make it a multiple of 31
		// zlib format uses big-endian order
		return  std::array<std::byte, 2>{
			std::byte((head >> 8) & 0xff),
			std::byte(head & 0xff) };
	}

	std::array<std::byte, 4> get_trailer(uint32_t check)
	{
		// zlib format uses big-endian order
		return std::array<std::byte, 4>{
			std::byte((check >> 24) & 0xff),
				std::byte((check >> 16) & 0xff),
				std::byte((check >> 8) & 0xff),
				std::byte(check & 0xff)};
	}

	/// \return <output_length, checksum>
	std::tuple<uLong, uint32_t> compress_worker_zlib_part(
		const std::byte* in,
		std::byte* out,
		size_t insize,
		size_t outsize,
		int level,
		bool lastpart)
	{
		z_stream strm{
			.next_in = reinterpret_cast<const Bytef*>(in),
			.avail_in = chkcast<uInt>(insize),
			.next_out = reinterpret_cast<Bytef*>(out),
			.avail_out = chkcast<uInt>(outsize),
			.zalloc = nullptr,
			.zfree = nullptr,
			.opaque = nullptr };
		checkErr(Z_OK == deflateInit2(
			&strm, level, Z_DEFLATED, -windowBits, memLevel, Z_DEFAULT_STRATEGY));
		if (lastpart)
		{
			checkErr(Z_STREAM_END == deflate(&strm, Z_FINISH));
		}
		else
		{
			checkErr(Z_OK == deflate(&strm, Z_FULL_FLUSH));
		}
		uLong outlen = strm.total_out;
		uint32_t check = adler32(0, nullptr, 0);
		check = adler32(check, reinterpret_cast<const Bytef*>(in), chkcast<uInt>(insize));
		// deflateEnd is supposed to return Z_OK
		// but its seems to return a strange value
		// so I don't check it
		// checkErr(Z_OK == deflateEnd(&strm));
		deflateEnd(&strm);
		return { outlen, check };
	}

	/// \return <output_data, output_length, checksum>
	std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t> compress_worker_zopfli_part(
		const std::byte* in,
		size_t instart,
		size_t inend,
		bool lastpart)
	{
		constexpr ZopfliOptions options{
			.verbose = 0,
			.verbose_more = 0,
			.numiterations = 5,
			.blocksplitting = 1 };
		constexpr int btype_blocks_with_dynamic_tree = 2;

		unsigned char bits = 0;
		std::byte* out_c_ptr = nullptr;
		size_t outsize = 0;
		ZopfliDeflatePart(&options,
			btype_blocks_with_dynamic_tree,
			lastpart,
			reinterpret_cast<const unsigned char*>(in),
			instart,
			inend,
			&bits,
			reinterpret_cast<unsigned char**>(&out_c_ptr),
			&outsize);
		auto out = unique_c_ptr<std::byte[]>(reinterpret_cast<std::byte*>(
			checkNull(realloc(out_c_ptr, outsize + 10))));
		if (!lastpart) {
			bits &= 7;
			if (bits == 0 || bits > 5)
			{
				out[outsize++] = std::byte(0);
			}
			out[outsize++] = std::byte(0);
			out[outsize++] = std::byte(0);
			out[outsize++] = std::byte(0xff);
			out[outsize++] = std::byte(0xff);

			// two markers when independent
			out[outsize++] = std::byte(0);
			out[outsize++] = std::byte(0);
			out[outsize++] = std::byte(0);
			out[outsize++] = std::byte(0xff);
			out[outsize++] = std::byte(0xff);
		}

		uint32_t check = adler32(0, nullptr, 0);
		check = adler32(check, reinterpret_cast<const Bytef*>(in) + instart,
			chkcast<uInt>(inend - instart));

		return { std::move(out), outsize, check };
	}

	std::vector<std::byte> compress_worker_zlib_nonpart(
		const std::byte* data, size_t inputsize, int level)
	{
		uLongf compressed_size = compressBound(uLong(inputsize));
		std::vector<std::byte> r(compressed_size);
		checkErr(Z_OK == compress2(
			reinterpret_cast<Bytef*>(r.data()),
			&compressed_size,
			reinterpret_cast<const Bytef*>(data),
			uLong(inputsize),
			level));
		r.resize(compressed_size);
		r.shrink_to_fit();
		return r;
	}

	std::future<std::vector<std::byte>> compress_zlib_part(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level)
	{
		const size_t partnum = CEIL_DIV(inputsize, partinsize);
		const uLong partoutsize = compressBound(partinsize);
		std::vector<std::byte> compressed(partnum * partoutsize);
		std::vector<std::future<std::tuple<uLong, uint32_t>>> futures;
		for (size_t i = 0; i < partnum; i++)
		{
			const bool lastpart = partnum == i + 1;
			futures.push_back(pool.enqueue(compress_worker_zlib_part,
				data + i * partinsize,
				compressed.data() + i * partoutsize,
				lastpart ? inputsize - i * partinsize : partinsize,
				partoutsize,
				level,
				lastpart));
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
			auto p = compressed.begin() + header.size();
			// merge data
			for (size_t i = 0; i < futures.size(); i++)
			{
				auto [outlen, check] = futures[i].get();
				// we have to use memmove here, because the memory range may be overlapped
				memmove(&(*p), compressed.data() + i * size_t(partoutsize), outlen);
				p += outlen;
				check_comb = adler32_comb(check_comb,
					check,
					futures.size() == i + 1 ? inputsize - i * partinsize : partinsize);
			}
			// write header
			std::copy(header.begin(), header.end(), compressed.begin());
			// write trailer
			auto trailer = get_trailer(check_comb);
			std::copy(trailer.begin(), trailer.end(), p);
			compressed.resize(p + trailer.size() - compressed.begin());
			compressed.shrink_to_fit();
			return compressed;
		});
	}

	std::future<std::vector<std::byte>> compress_zopfli_part(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level)
	{
		const size_t partnum = CEIL_DIV(inputsize, partinsize);
		std::vector<std::future<std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t>>>
			futures;
		for (size_t i = 0; i < partnum; i++)
		{
			const bool lastpart = partnum == i + 1;
			futures.push_back(pool.enqueue(compress_worker_zopfli_part,
				data,
				i * partinsize,
				i * partinsize +
				(lastpart ? inputsize - i * partinsize : partinsize),
				lastpart));
		}
		return std::async(std::launch::deferred,
			[futures = std::move(futures),
			level,
			inputsize]() mutable
		{
			// get all compressed part
			// <output_data, output_length, checksum>
			std::vector<std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t>>
				worker_results;
			worker_results.reserve(futures.size());
			for (auto& f : futures)
			{
				worker_results.push_back(f.get());
			}
			// get compressed size and allocate a vector
			const size_t compressed_size = std::accumulate(
				worker_results.begin(),
				worker_results.end(), size_t(0),
				[](auto sum, auto& res) {return sum + std::get<1>(res); });
			auto header = get_header(level);
			// reserve bytes for header & trailer
			std::vector<std::byte> compressed(header.size() +
				compressed_size +
				std::tuple_size<decltype(get_trailer(0))>::value
			);
			auto p = compressed.begin() + header.size();
			
			uint32_t check_comb = adler32(0, nullptr, 0);
			// merge data
			for (size_t i = 0; i < worker_results.size(); i++)
			{
				const auto& [outdata, outlen, check] = worker_results[i];
				std::copy_n(outdata.get(), outlen, p);
				p += outlen;
				check_comb = adler32_comb(check_comb,
					check,
					futures.size() == i + 1 ? inputsize - i * partinsize : partinsize);
			}

			// write header
			std::copy(header.begin(), header.end(), compressed.begin());
			// write trailer
			auto trailer = get_trailer(check_comb);
			std::copy(trailer.begin(), trailer.end(), p);
			return compressed;
		});
	}

	std::vector<std::byte> compress_worker_zopfli_nonpart(
		const std::byte* data, size_t inputsize)
	{
		constexpr ZopfliOptions options{
			.verbose = 0,
			.verbose_more = 0,
			.numiterations = 5,
			.blocksplitting = 1,
		};
		unsigned char* out_c_ptr = nullptr;
		size_t outsize = 0;
		ZopfliCompress(&options,
			ZOPFLI_FORMAT_ZLIB,
			reinterpret_cast<const unsigned char*>(data),
			inputsize,
			&out_c_ptr,
			&outsize);
		const auto out = unique_c_ptr<std::byte[]>(reinterpret_cast<std::byte*>(out_c_ptr));
		std::vector<std::byte> r(outsize);
		std::copy_n(out.get(), outsize, r.begin());
		return r;
	}

	std::vector<std::byte> uncompress_worker(
		const std::byte* data, size_t inputsize, size_t outputsize)
	{
		std::vector<std::byte> r(outputsize);
		auto ul_outputsize = uLongf(outputsize);
		checkErr(Z_OK == uncompress(
			reinterpret_cast<Bytef*>(r.data()),
			&ul_outputsize,
			reinterpret_cast<const Bytef*>(data),
			uLong(inputsize)) && ul_outputsize == outputsize);
		return r;
	}
}

namespace compressor
{
	std::future<std::vector<std::byte>> compress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level)
	{
		if (inputsize >= partboundsize)
		{
			if (level <= 9) return compress_zlib_part(pool, data, inputsize, level);
			else return compress_zopfli_part(pool, data, inputsize, level);
		}
		else
		{
			if (level <= 9) return pool.enqueue(compress_worker_zlib_nonpart, data, inputsize, level);
			else return pool.enqueue(compress_worker_zopfli_nonpart, data, inputsize);
		}
	}

	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize)
	{
		return pool.enqueue(uncompress_worker, data, inputsize, outputsize);
	}
}
