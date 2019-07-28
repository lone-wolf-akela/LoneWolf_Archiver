#pragma once

#if defined(_WIN32)

#include <cstddef>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

#include <Windows.h>

#include <json/json.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "../exceptions/exceptions.h"
#include "../core.h"

namespace server
{
	class JsonServer
	{
	public:
		JsonServer(const std::string& pipename);
		~JsonServer();
		void start_listen();
	private:		
		HANDLE _hPipe = INVALID_HANDLE_VALUE;
		std::shared_ptr<spdlog::logger> _logger = spdlog::stdout_color_mt("pipe");

		Json::Value _read();
		void _write(const Json::Value msg);
		void _write(const std::string& msg);
		Json::Value _msgStr2Json(const std::string& msg);
	};
	void start(const std::string& pipename);
}
#endif
