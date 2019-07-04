#pragma once
#include <cstddef>

#include <type_traits>
#include <utility>
#include <vector>
#include <tuple>

namespace stream
{
	class OptionalOwnerBuffer
	{
	public:
		OptionalOwnerBuffer() noexcept;
		OptionalOwnerBuffer(const OptionalOwnerBuffer&) = delete;
		OptionalOwnerBuffer& operator=(const OptionalOwnerBuffer&) = delete;
		OptionalOwnerBuffer(OptionalOwnerBuffer&& o) noexcept;
		OptionalOwnerBuffer& operator=(OptionalOwnerBuffer&& o) noexcept;
		OptionalOwnerBuffer(std::vector<std::byte>&& in) noexcept;
		OptionalOwnerBuffer& operator=(std::vector<std::byte>&& in) noexcept;
		OptionalOwnerBuffer(std::byte* in) noexcept;
		OptionalOwnerBuffer& operator=(std::byte* in) noexcept;
		OptionalOwnerBuffer(const std::byte* in) noexcept;
		OptionalOwnerBuffer& operator=(const std::byte* in) noexcept;

		~OptionalOwnerBuffer();

		std::byte* get() noexcept;
		const std::byte* get_const() const noexcept;
		void reset();
	private:
		enum { v, p, pc } mode;
		union D
		{
			std::vector<std::byte> v;
			std::byte* p;
			const std::byte* pc;
			D() {}
			~D() {}
		} data;
	};

	class FileStream
	{
	public:
		virtual ~FileStream() = default;

		virtual size_t read(void* dst, size_t length) = 0;
		virtual std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t length) = 0;
		virtual void write(const void* src, size_t length) = 0;

		//these two do not move the read ptr
		virtual size_t read(size_t pos, void* dst, size_t length) = 0;
		virtual std::tuple<OptionalOwnerBuffer, size_t>
			optionalOwnerRead(size_t pos, size_t length) = 0;

		virtual void setpos(size_t pos) = 0;
		virtual size_t getpos(void) = 0;
		virtual void movepos(ptrdiff_t diff) = 0;
	};
}
