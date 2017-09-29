#pragma once


#include <stdexcept>
#include <string>

class FileIoError : public std::runtime_error
{
private:
	std::string what_message;
public:
	FileIoError(std::string message) : 
		runtime_error("File IO Error."),
		what_message(message)
	{}

	const char* what() const noexcept override
	{
		return what_message.c_str();
	}
};