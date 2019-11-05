#pragma once
#include <cstdlib>
#include <cassert>
#include <cstddef>

#include <memory>
#include <limits>
#include <array>
#include <concepts>

template<typename T>
class unique_c_ptr : public std::unique_ptr<T, decltype(std::free)*>
{
public:
	using base_type = std::unique_ptr<T, decltype(std::free)*>;
	unique_c_ptr() : base_type(nullptr, std::free) {}
	unique_c_ptr(T* p) : base_type(p, std::free) {}
};

template<typename T>
class unique_c_ptr<T[]> : public std::unique_ptr<T[], decltype(std::free)*>
{
public:
	using base_type = std::unique_ptr<T[], decltype(std::free)*>;
	unique_c_ptr() : base_type(nullptr, std::free) {}
	unique_c_ptr(T* p) : base_type(p, std::free) {}
};

template<std::integral T>
constexpr T CEIL_DIV(T x, T y)
{
	return x / y + ((x % y) != 0);
}

template<std::integral Tout, std::integral Tin>
constexpr Tout chkcast(Tin in)
{
	assert(in <= std::numeric_limits<Tout>::max());
	return Tout(in);
}

// this will be in c++20
template <class To, class From>
typename std::enable_if<
(sizeof(To) == sizeof(From)) &&
std::is_trivially_copyable<From>::value &&
std::is_trivial<To>::value,
// this implementation requires that To is trivially default constructible
To>::type
// constexpr support needs compiler magic
bit_cast(const From& src) noexcept
{
	To dst;
	std::memcpy(&dst, &src, sizeof(To));
	return dst;
}
