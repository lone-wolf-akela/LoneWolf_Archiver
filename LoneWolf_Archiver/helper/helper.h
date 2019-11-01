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

template<std::integral T>
constexpr std::array<std::byte, sizeof(T)> ToBigEndian(T in)
{
	std::array<std::byte, sizeof(T)> out;
	for (size_t i = 0; i < sizeof(T); i++)
	{
		out[sizeof(T) - i] = std::byte((in >> (8 * i)) & 0xff);
	}
	return out;
}

template<std::integral T>
constexpr T FromBigEndian(std::array<std::byte, sizeof(T)> in)
{
	T out = 0;
	for (size_t i = 0; i < sizeof(T); i++)
	{
		out |= (T(in[sizeof(T) - i]) << (8 * i));
	}
	return out;
}
