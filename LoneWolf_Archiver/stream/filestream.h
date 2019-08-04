#pragma once
#include <cstddef>

#include <type_traits>
#include <utility>
#include <vector>
#include <tuple>
#include <variant>

namespace stream
{
	class OptionalOwnerBuffer
	{
	public:
		OptionalOwnerBuffer() noexcept = default;
		OptionalOwnerBuffer(const OptionalOwnerBuffer&) = delete;
		OptionalOwnerBuffer& operator=(const OptionalOwnerBuffer&) = delete;
		template<typename T, std::enable_if_t<
			std::is_same_v<T, const std::byte*> ||
			std::is_same_v<T, std::byte*> ||
			std::is_same_v<T, std::vector<std::byte>>>* = nullptr>
			explicit OptionalOwnerBuffer(T && in) noexcept : data(std::move(in)) {}
		OptionalOwnerBuffer(OptionalOwnerBuffer&&) noexcept = default;
		template<typename T, std::enable_if_t<
			std::is_same_v<T, const std::byte*> ||
			std::is_same_v<T, std::byte*> ||
			std::is_same_v<T, std::vector<std::byte>>>* = nullptr >
			OptionalOwnerBuffer & operator=(T && in) noexcept
		{
			data = std::move(in);
			return *this;
		}
		OptionalOwnerBuffer& operator=(OptionalOwnerBuffer&&) noexcept = default;
		template<typename T, std::enable_if_t<
			std::is_same_v<T, const std::byte*> ||
			std::is_same_v<T, std::byte* >>* = nullptr>
			OptionalOwnerBuffer & operator=(T & in) noexcept
		{
			data = in;
			return *this;
		}
		std::byte* get();
		const std::byte* get_const() const;
		void reset();
	private:
		std::variant<const std::byte*, std::byte*, std::vector<std::byte>> data;
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
