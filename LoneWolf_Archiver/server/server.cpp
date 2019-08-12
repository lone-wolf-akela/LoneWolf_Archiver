#include <algorithm>
#include <sstream>

#include "server.h"

#if defined(_WIN32)
namespace
{
	class ErrorClientDisconnected : public std::exception {};
}
namespace server
{
	JsonServer::JsonServer(const std::string& pipename)
	{
		auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		_logger = std::make_shared<spdlog::logger>("pipe", sink);

		_hPipe = CreateNamedPipeA(
			(R"(\\.\pipe\)" + pipename).c_str(),	// pipe name 
			PIPE_ACCESS_DUPLEX,						// read/write access 
			PIPE_TYPE_BYTE |						// message type pipe 
			PIPE_READMODE_BYTE |					// message-read mode 
			PIPE_WAIT |								// blocking mode
			PIPE_REJECT_REMOTE_CLIENTS,
			1,										// max instances  
			BUFFER_SIZE,							// output buffer size 
			BUFFER_SIZE,							// input buffer size 
			0,	// client time-out, 0 = default time-out of 50 milliseconds 
			nullptr);								// default security attribute	
		if (INVALID_HANDLE_VALUE == _hPipe)
		{
			throw SystemApiError(
				"Error happened when calling `CreateNamedPipeA', error code: " +
				std::to_string(GetLastError()));
		}
		_logger->info("Pipe created.");
	}
	JsonServer::~JsonServer()
	{
		CloseHandle(_hPipe);
	}
	void JsonServer::start_listen()
	{
		// Wait for the client to connect; if it succeeds, 
		// the function returns a nonzero value. It is still good if the function
		// returns zero but GetLastError returns ERROR_PIPE_CONNECTED.
		DWORD errcode;
		const bool bConnected = ConnectNamedPipe(_hPipe, nullptr) ? true :
			((errcode = GetLastError()) == ERROR_PIPE_CONNECTED);
		if (!bConnected)
		{
			throw SystemApiError(
				"Error happened when calling `ConnectNamedPipe', error code: " +
				std::to_string(errcode));
		}
		try
		{
			{
				Json::Value msg_read = _read();
				const auto msg_str = msg_read["msg"].asString();
				if ("hello" == msg_str)
				{
					_write("hello");
					_logger->info("connected client.");
				}
				else
				{
					throw ServerError("Unknown client message: " + msg_str);
				}
			}
			while (true)
			{
				Json::Value msg_json = _read();
				auto msg_str = msg_json["msg"].asString();
				if ("bye" == msg_str)
				{
					_write("bye");
					// Flush the pipe to allow the client to read the pipe's contents 
					// before disconnecting. Then disconnect the pipe
					FlushFileBuffers(_hPipe);
					DisconnectNamedPipe(_hPipe);
					_logger->info("connection closed.");

					return;
				}
				else if ("open" == msg_str)
				{
					_logger->info("open big file.");
					std::filesystem::path filepath(msg_json["param"]["path"].asString());
					_file = std::make_unique<archive::Archive>(filepath.filename().string());
					_file->open(filepath, archive::Archive::Mode::Read);
					_write("ok");
				}
				else if ("extract_all" == msg_str)
				{
					throw NotImplementedError();
				}
				else if ("extract_file" == msg_str)
				{
					throw NotImplementedError();
				}
				else if ("extract_folder" == msg_str)
				{
					throw NotImplementedError();
				}
				else if ("extract_toc" == msg_str)
				{
					throw NotImplementedError();
				}
				else if ("get_filetree" == msg_str)
				{
					_logger->info("send file tree.");
					_write("filetree", _file->getFileTree());
				}
				else if ("generate" == msg_str)
				{
					_logger->info("generate new big file.");
					auto param = msg_json["param"];
					std::vector<std::u8string> ignorelist;
					for (const auto& i : param["ignorelist"])
					{
						ignorelist.emplace_back(
							reinterpret_cast<const char8_t*>(i.asString().c_str()));
					}
					core::generate(param["root"].asString(),
						param["allinone"].asBool(),
						param["archive"].asString(),
						param["threadnum"].asInt(),
						param["encryption"].asBool(),
						param["compresslevel"].asInt(),
						param["keepsign"].asBool(),
						ignorelist);
					_write("ok");
				}
				else
				{
					throw ServerError("Unknown client message: " + msg_str);
				}
			}
		}
		catch (ErrorClientDisconnected&)
		{
			_logger->warn("connection lost.");
			return;
		}
	}
	Json::Value JsonServer::_read()
	{
		std::stringstream strm;
		
		while (true)
		{
			const auto zero_pos = std::find(_buffer, _buffer + _bytes_in_buffer, 0);
			if (_buffer + _bytes_in_buffer != zero_pos)
			{
				//found 0 in buffer
				strm << std::string_view(_buffer, zero_pos - _buffer);
				_bytes_in_buffer -= DWORD(zero_pos - _buffer + 1);
				memmove(_buffer, zero_pos + 1, _buffer + BUFFER_SIZE - zero_pos - 1);
				break;
			}
			else
			{
				strm << std::string_view(_buffer, _bytes_in_buffer);
			}
			const BOOL bSuccess = ReadFile(
				_hPipe,					// handle to pipe 
				_buffer,					// buffer to receive data 
				BUFFER_SIZE,			// size of buffer 
				&_bytes_in_buffer,			// number of bytes read 
				nullptr);				// default security attribute	
			if (!bSuccess || _bytes_in_buffer == 0)
			{
				const auto errcode = GetLastError();
				if (ERROR_BROKEN_PIPE == errcode)
				{
					throw ErrorClientDisconnected();
				}
				else
				{
					throw SystemApiError(
						"Error happened when calling `ReadFile', error code: " +
						std::to_string(errcode));
				}
			}
		}
		Json::Value msg;
		strm >> msg;
		return msg;
	}
	void JsonServer::_write(const Json::Value& msg)
	{
		std::stringstream strm;
		strm << msg;
		const std::string str = strm.str();

		DWORD bytes_written;
		const BOOL bSuccess = WriteFile(
			_hPipe,					// handle to pipe 
			str.c_str(),			// buffer to write from 
			DWORD(str.length() + 1),// number of bytes to write 
			&bytes_written,			// number of bytes written 
			nullptr);				// default security attribute

		if (!bSuccess || str.length() + 1 != bytes_written)
		{
			throw SystemApiError(
				"Error happened when calling `WriteFile', error code: " +
				std::to_string(GetLastError()));
		}
	}
	void JsonServer::_write(const std::string& msg)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		_write(root);
	}
	void JsonServer::_write(const std::string& msg, const Json::Value& param)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		root["param"] = param;
		_write(root);
	}
	void JsonServer::_write(const char* msg)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		_write(root);
	}
	void JsonServer::_write(const char* msg, const Json::Value& param)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		root["param"] = param;
		_write(root);
	}
	void start(const std::string& pipename)
	{
		JsonServer s(pipename);
		s.start_listen();
	}
}
#endif
