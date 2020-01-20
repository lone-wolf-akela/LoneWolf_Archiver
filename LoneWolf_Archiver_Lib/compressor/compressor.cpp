#include <array>
#include <algorithm>
#include <numeric>

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

	constexpr int windowBits = 15; // larger = better compression & more memory use
	constexpr int memLevel = 9; // similar to windowBits, larger = better compression & memory use

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
		sum2 += ((adler1 >> 16)& LOW16) + ((adler2 >> 16)& LOW16) + BASE - rem;
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
		return bit_cast<std::array<std::byte, 2>>(boost::endian::native_to_big(head));
	}
	
	/// \return <output_data, output_length, checksum>
	std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t> compress_worker_zlib_part(
		const std::byte* in,
		size_t insize,
		size_t outsize,
		int level,
		bool lastpart)
	{
		unique_c_ptr<std::byte[]> out(reinterpret_cast<std::byte*>(malloc(outsize)));
		z_stream strm{
			.next_in = reinterpret_cast<const Bytef*>(in),
			.avail_in = chkcast<uInt>(insize),
			.next_out = reinterpret_cast<Bytef*>(out.get()),
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
		size_t output_length = strm.total_out;
		uint32_t check = adler32(0, nullptr, 0);
		check = adler32(check, reinterpret_cast<const Bytef*>(in), chkcast<uInt>(insize));
		// deflateEnd is supposed to return Z_OK
		// but its seems to return a strange value
		// so I don't check it
		// checkErr(Z_OK == deflateEnd(&strm));
		deflateEnd(&strm);
		return { std::move(out), output_length, check };
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
		std::vector<std::byte> r(out.get(), out.get() + outsize);
		return r;
	}
	
	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize)
	{
		return pool.enqueue(uncompress_worker, data, inputsize, outputsize);
	}
}
