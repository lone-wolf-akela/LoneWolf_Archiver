#pragma once
#include <cstdlib>

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
T CEIL_DIV(T x, T y)
{
	return x / y + ((x % y) != 0);
}
