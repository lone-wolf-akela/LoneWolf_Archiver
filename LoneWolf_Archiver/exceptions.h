#pragma once


#include <stdexcept>
#include <string>

class FileIoError : public std::runtime_error
{
public:
	explicit FileIoError(std::string const& message):
		runtime_error(message)
	{}
};

class NotImplementedError : public std::runtime_error
{
public:
	NotImplementedError(void):
		runtime_error("Function not implemented")
	{}
};

class OutOfRangeError : public std::runtime_error
{
public:
	OutOfRangeError(void) :
		runtime_error("Out of range.")
	{}
};

class FormatError : public std::runtime_error
{
public:
	explicit FormatError(std::string const& message) :
		runtime_error(message)
	{}
};

class ZlibError : public std::runtime_error
{
public:
	explicit ZlibError(std::string const& message = "") :
		runtime_error(message)
	{}
};

class FatalError : public std::runtime_error
{
public:
	explicit FatalError(std::string const& message = "") :
		runtime_error(message)
	{}
};