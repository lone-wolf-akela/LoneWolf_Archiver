#pragma once

#if defined(_WIN32)

#include <memory>
#include <string>

#include <Windows.h>

#include <json/json.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../exceptions/exceptions.h"
#include "../core.h"

namespace server
{
	constexpr DWORD BUFFER_SIZE = 1024;
	class JsonServer
	{
	public:
		JsonServer(const std::string& pipename);
		JsonServer(const JsonServer&) = delete;
		JsonServer& operator=(const JsonServer&) = delete;
		JsonServer(JsonServer&&) = delete;
		JsonServer& operator=(JsonServer&&) = delete;
		~JsonServer();
		
		void start_listen();
	private:		
		char _buffer[BUFFER_SIZE + 1] = { 0 };
		DWORD _bytes_in_buffer = 0;
		HANDLE _hPipe = INVALID_HANDLE_VALUE;
		std::shared_ptr<spdlog::logger> _logger;
		std::unique_ptr<archive::Archive> _file;

		Json::Value _read();
		void _write(const Json::Value& msg);
		void _write(const std::string& msg);
		void _write(const std::string& msg, const Json::Value& param);
		void _write(const char* msg);
		void _write(const char* msg, const Json::Value& param);
	};
	void start(const std::string& pipename);
}
#endif
