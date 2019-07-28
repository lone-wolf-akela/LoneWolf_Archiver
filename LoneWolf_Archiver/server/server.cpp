#include "server.h"
#if defined(_WIN32)
namespace
{
	constexpr DWORD BUFFER_SIZE = 1024;
	class ErrorClientDisconnected {};
}
namespace server
{
	JsonServer::JsonServer(const std::string& pipename)
	{
		_hPipe = CreateNamedPipeA(
			("\\\\.\\pipe\\" + pipename).c_str(),	// pipe name 
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
		bool bConnected = ConnectNamedPipe(_hPipe, nullptr) ? true :
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
				auto msg_str = msg_read["msg"].asString();
				if ("hello" == msg_str)
				{
					_write(std::string("hello"));
					_logger->info("connected client.");
				}
				else
				{
					throw ServerError("Unknown client message: " + msg_str);
				}
			}
			while (true)
			{
				Json::Value msg_read = _read();
				auto msg_str = msg_read["msg"].asString();
				if ("bye" == msg_str)
				{
					_write(std::string("bye"));
					// Flush the pipe to allow the client to read the pipe's contents 
					// before disconnecting. Then disconnect the pipe
					FlushFileBuffers(_hPipe);
					DisconnectNamedPipe(_hPipe);
					_logger->info("connection closed.");

					return;
				}
				else if ("open" == msg_str)
				{

				}
				else if ("extract_all" == msg_str)
				{

				}
				else if ("extract_file" == msg_str)
				{

				}
				else if ("extract_folder" == msg_str)
				{

				}
				else if ("extract_toc" == msg_str)
				{

				}
				else if ("get_filetree" == msg_str)
				{

				}
				else if ("generate" == msg_str)
				{
					_logger->info("generate new big file.");
					std::vector<std::u8string> ignorelist;
					for (const auto& i : msg_read["param"]["ignorelist"])
					{
						ignorelist.emplace_back(
							reinterpret_cast<const char8_t*>(i.asString().c_str()));
					}
					core::generate(msg_read["param"]["root"].asString(),
						msg_read["param"]["allinone"].asBool(),
						msg_read["param"]["archive"].asString(),
						msg_read["param"]["threadnum"].asInt(),
						msg_read["param"]["encryption"].asBool(),
						msg_read["param"]["compresslevel"].asInt(),
						msg_read["param"]["keepsign"].asBool(),
						ignorelist);
				}
				else
				{
					throw ServerError("Unknown client message: " + msg_str);
				}
			}
		}
		catch (ErrorClientDisconnected)
		{
			_logger->warn("connection lost.");
			return;
		}
	}
	Json::Value JsonServer::_read()
	{
		std::stringstream strm;
		char buffer[BUFFER_SIZE + 1] = { 0 };
		DWORD bytes_read;
		while (true)
		{
			BOOL bSuccess = ReadFile(
				_hPipe,					// handle to pipe 
				buffer,					// buffer to receive data 
				BUFFER_SIZE,			// size of buffer 
				&bytes_read,			// number of bytes read 
				nullptr);				// default security attribute	
			if (!bSuccess || bytes_read == 0)
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
			strm << buffer;
			if (0 == buffer[bytes_read - 1]) break;
		}
		Json::Value msg;
		strm >> msg;
		return msg;
	}
	void JsonServer::_write(const Json::Value msg)
	{
		std::stringstream strm;
		strm << msg;
		std::string str = strm.str();

		DWORD bytes_written;
		BOOL bSuccess = WriteFile(
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
		_write(_msgStr2Json(msg));
	}
	Json::Value JsonServer::_msgStr2Json(const std::string& msg)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		return root;
	}

	void start(const std::string& pipename)
	{
		JsonServer s(pipename);
		s.start_listen();
	}
}
#endif
