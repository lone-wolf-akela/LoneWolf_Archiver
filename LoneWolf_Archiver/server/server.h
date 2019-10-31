#pragma once

#include <memory>
#include <string>
#include <array>

#include <json/json.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <boost/asio.hpp>

#include "../exceptions/exceptions.h"
#include "../core.h"

namespace server
{
	class JsonServer
	{
	public:
		JsonServer();
		JsonServer(const JsonServer&) = delete;
		JsonServer& operator=(const JsonServer&) = delete;
		JsonServer(JsonServer&&) = delete;
		JsonServer& operator=(JsonServer&&) = delete;
		~JsonServer() = default;
		
		void start_listen();
	private:
		using tcp = boost::asio::ip::tcp;
		
		std::shared_ptr<spdlog::logger> _logger = nullptr;
		std::unique_ptr<archive::Archive> _file = nullptr;

		boost::asio::io_context _io_context;
		tcp::acceptor _acceptor;
		tcp::socket _socket;
		
		Json::Value _read();
		void _write(const Json::Value& msg);
		void _write(const std::string& msg);
		void _write(const std::string& msg, const Json::Value& param);
		void _write(const char* msg);
		void _write(const char* msg, const Json::Value& param);
	};
	void start();
}

