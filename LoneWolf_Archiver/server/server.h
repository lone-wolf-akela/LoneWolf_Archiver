#pragma once

#include <memory>
#include <string>
#include <optional>

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
		
		std::shared_ptr<spdlog::logger> _logger;
		std::optional<archive::Archive> _file{};

		boost::asio::io_context _io_context;
		tcp::acceptor _acceptor;
		tcp::socket _socket;

		std::unique_ptr<Json::StreamWriter> _jsonwriter{};
		
		Json::Value _read();
		void _write(const Json::Value& msg);
		void _write(const std::string& msg);
		void _write(const std::string& msg, const Json::Value& param);
		void _write(const char* msg);
		void _write(const char* msg, const Json::Value& param);
	};
	void start();
}

