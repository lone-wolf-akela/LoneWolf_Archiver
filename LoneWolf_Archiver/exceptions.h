#pragma once


#include <stdexcept>
#include <string>

class FileIoError : public std::runtime_error
{
public:
	FileIoError(std::string message) : 
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
	FormatError(std::string message) :
		runtime_error(message)
	{}
};