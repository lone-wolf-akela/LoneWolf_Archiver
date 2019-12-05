#if !defined(LONEWOLF_ARCHIVER_LIB_STREAM_FILESTREAM_H)
#define LONEWOLF_ARCHIVER_LIB_STREAM_FILESTREAM_H

#include <cstddef>

#include <concepts>
#include <utility>
#include <vector>
#include <tuple>
#include <variant>

namespace stream
{
	template <typename T>
	concept OptionalData = std::same_as<T, const std::byte*> ||
		std::same_as<T, std::byte*> ||
		std::same_as<T, std::vector<std::byte> >;
	template <typename T>
	concept OptionalPointer = std::same_as<T, const std::byte*> ||
		std::same_as<T, std::byte*>;
	
	class OptionalOwnerBuffer
	{
	public:
		OptionalOwnerBuffer() noexcept = default;
		OptionalOwnerBuffer(const OptionalOwnerBuffer&) = delete;
		OptionalOwnerBuffer& operator=(const OptionalOwnerBuffer&) = delete;
		template<OptionalData T>
			explicit OptionalOwnerBuffer(T && in) noexcept : data(std::move(in)) {}
		OptionalOwnerBuffer(OptionalOwnerBuffer&&) noexcept = default;
		template<OptionalData T>
			OptionalOwnerBuffer & operator=(T && in) noexcept
		{
			data = std::move(in);
			return *this;
		}
		OptionalOwnerBuffer& operator=(OptionalOwnerBuffer&&) noexcept = default;
		template<OptionalPointer T>
			OptionalOwnerBuffer & operator=(T & in) noexcept
		{
			data = in;
			return *this;
		}
		~OptionalOwnerBuffer() = default;
		[[nodiscard]] std::byte* get();
		[[nodiscard]] const std::byte* get_const() const;
		void reset();
	private:
		std::variant<const std::byte*, std::byte*, std::vector<std::byte>> data;
	};

	class FileStream
	{
	public:
		FileStream() = default;
		FileStream(const FileStream&) = delete;
		FileStream& operator=(const FileStream&) = delete;
		FileStream(FileStream&&) = delete;
		FileStream& operator=(FileStream&&) = delete;
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
		virtual size_t getpos() = 0;
		virtual void movepos(ptrdiff_t diff) = 0;
	};
}

#endif
