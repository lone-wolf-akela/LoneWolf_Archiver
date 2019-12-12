#if !defined(LONEWOLF_ARCHIVER_LIB_COMPRESSOR_COMPRESSOR_H)
#define LONEWOLF_ARCHIVER_LIB_COMPRESSOR_COMPRESSOR_H

#include <cstddef>
#include <cstdint>

#include <vector>
#include <concepts>
#include <memory>
#include <type_traits>
#include <optional>

#include <boost/endian/conversion.hpp>
#define ZLIB_CONST // this make z_stream.next_in const
#include <zlib.h>

// for multithread support, we need some internal functions in zopfli.
// so we cannot just use a package manager to install the lib,
// but we have to include its src code in this project
#include "../zopfli/src/zopfli/deflate.h"
#include "../zopfli/src/zopfli/zopfli.h"

#include "../ThreadPool/ThreadPool.hpp"
#include "../exceptions/exceptions.h"
#include "../helper/helper.h"

namespace compressor
{
	constexpr size_t partinsize = 8 * 1024 * 1024; // 4MB per part
	constexpr size_t partboundsize = 12 * 1024 * 1024; // file smaller than this won't be partitioned

	std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t> compress_worker_zlib_part(
		const std::byte* in,
		size_t insize,
		size_t outsize,
		int level,
		bool lastpart);
	std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t> compress_worker_zopfli_part(
		const std::byte* in,
		size_t instart,
		size_t inend,
		bool lastpart);
	std::vector<std::byte> compress_worker_zlib_nonpart(
		const std::byte* data, size_t inputsize, int level);
	std::vector<std::byte> compress_worker_zopfli_nonpart(
		const std::byte* data, size_t inputsize);
	uint32_t adler32_comb(uint32_t adler1, uint32_t adler2, size_t len2);
	std::array<std::byte, 2> get_header(int level);

	template<std::invocable< std::vector<std::byte>&& > Functor>
	auto compress_part(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level, Functor&& callback)
	{
		using return_type = std::invoke_result_t<Functor, std::vector<std::byte>&&>;
		
		const size_t partnum = CEIL_DIV(inputsize, partinsize);
		const uLong partoutsize = compressBound(partinsize);
		auto parts_remained = std::make_shared<std::atomic_size_t>(partnum);
		auto partial_result = std::make_shared<std::vector<std::tuple<unique_c_ptr<std::byte[]>, size_t, uint32_t>>>(partnum);
		auto shared_callback = std::make_shared<Functor>(std::move(callback));
		std::vector<std::future<std::optional<return_type>>> future_list;
		
		for (size_t i = 0; i < partnum; i++)
		{
			const bool lastpart = partnum == i + 1;
			future_list.emplace_back(pool.enqueue([
				data,
					partoutsize,
					i,
					parts_remained,
					lastpart,
					inputsize,
					level,
					partial_result,
					shared_callback
			] () mutable -> std::optional<return_type>
				{
					if (level <= 9)
					{
						(*partial_result)[i] = compress_worker_zlib_part(
							data + i * partinsize,
							lastpart ? inputsize - i * partinsize : partinsize,
							partoutsize,
							level,
							lastpart);
					}
					else
					{
						(*partial_result)[i] = compress_worker_zopfli_part(
							data,
							i * partinsize,
							i * partinsize +
							(lastpart ? inputsize - i * partinsize : partinsize),
							lastpart);
					}
					--(*parts_remained);

					if (*parts_remained != 0)
					{
						return std::nullopt;
					}
					else
					{
						uint32_t check_comb = adler32(0, nullptr, 0);
						const auto header = get_header(level);
						size_t compressed_len = header.size() + sizeof(check_comb);
						for(const auto& r: *partial_result)
						{
							compressed_len += std::get<1>(r);
						}
						std::vector<std::byte> combined_data(compressed_len);
						// write header
						auto p = std::copy(header.begin(), header.end(), combined_data.begin());
						// merge data
						for (size_t j = 0; j < partial_result->size(); j++)
						{
							const auto& output_data = std::get<0>((*partial_result)[j]);
							const auto output_length = std::get<1>((*partial_result)[j]);
							const auto checksum = std::get<2>((*partial_result)[j]);
							p = std::copy_n(output_data.get(), output_length, p);
							check_comb = adler32_comb(check_comb,	checksum,
								partial_result->size() == j + 1 ? inputsize - j * partinsize : partinsize);
						}
						// write trailer
						const auto trailer = bit_cast<std::array<std::byte, sizeof(check_comb)>>
							(boost::endian::native_to_big(check_comb));
						std::copy(trailer.begin(), trailer.end(), p);
						return std::make_optional((*shared_callback)(std::move(combined_data)));
					}
				}));
		}
		// combine these futures
		return std::async(std::launch::deferred,
			[future_list = std::move(future_list)]() mutable
		{
			std::optional<return_type> r;
			for (auto& future : future_list)
			{
				auto t = future.get();
				if (t.has_value())
				{
					r = std::move(t);
				}
			}
			return std::move(*r);
		});
	}
	
	template<std::invocable< std::vector<std::byte>&& > Functor>
	auto compress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, int level, Functor&& callback)
	{
		if (inputsize >= partboundsize)
		{
			return compress_part(pool, data, inputsize, level, std::move(callback));
		}
		else
		{
			if (level <= 9) return pool.enqueue([data, inputsize, level, callback = std::move(callback)]
				() mutable
				{
					return callback(std::move(compress_worker_zlib_nonpart(data, inputsize, level)));
				});
			else return pool.enqueue([data, inputsize, callback = std::move(callback)]
				() mutable
				{
					return callback(std::move(compress_worker_zopfli_nonpart(data, inputsize)));
				});
		}
	}
	std::future<std::vector<std::byte>> uncompress(
		ThreadPool& pool, const std::byte* data, size_t inputsize, size_t outputsize);
}

#endif
