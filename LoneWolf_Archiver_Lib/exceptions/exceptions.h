#if !defined(LONEWOLF_ARCHIVER_LIB_EXCEPTIONS_EXCEPTIONS_H)
#define LONEWOLF_ARCHIVER_LIB_EXCEPTIONS_EXCEPTIONS_H

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
	NotImplementedError():
		runtime_error("Function not implemented")
	{}
};

class OutOfRangeError : public std::runtime_error
{
public:
	OutOfRangeError() :
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

class UnkownError : public std::runtime_error
{
public:
	explicit UnkownError(std::string const& message) :
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

class EncodingError : public std::runtime_error
{
public:
	explicit EncodingError(std::string const& message = "") :
		runtime_error(message)
	{}
};

#endif
