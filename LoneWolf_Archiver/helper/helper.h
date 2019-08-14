#pragma once
#include <cstdlib>
#include <cassert>

#include <memory>
#include <limits>
#include <type_traits>

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

template<typename T>
requires std::is_integral_v<T>
constexpr T CEIL_DIV(T x, T y)
{
	return x / y + ((x % y) != 0);
}

template<typename Tout, typename Tin>
requires std::is_integral_v<Tout> && std::is_integral_v<Tin>
constexpr Tout chkcast(Tin in)
{
	assert(in <= std::numeric_limits<Tout>::max());
	return Tout(in);
}
