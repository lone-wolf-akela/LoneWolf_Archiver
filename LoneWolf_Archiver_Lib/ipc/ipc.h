#pragma once

#include <cstdint>

#include <memory>
#include <string>
#include <optional>

#include <json/json.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <boost/asio.hpp>

#include "../exceptions/exceptions.h"
#include "../core.h"

namespace ipc
{
	class IpcWorker
	{
	public:
		IpcWorker();
		IpcWorker(const IpcWorker&) = delete;
		IpcWorker& operator=(const IpcWorker&) = delete;
		IpcWorker(IpcWorker&&) = delete;
		IpcWorker& operator=(IpcWorker&&) = delete;
		~IpcWorker() = default;
		
		void connect(uint32_t port);
	private:
		using tcp = boost::asio::ip::tcp;
		
		std::shared_ptr<spdlog::logger> _logger;
		std::optional<archive::Archive> _file{};

		boost::asio::io_context _io_context;
		tcp::socket _socket;

		std::unique_ptr<Json::StreamWriter> _jsonwriter{};
		
		Json::Value _read();
		void _write(const Json::Value& msg);
		void _write(const std::string& msg);
		void _write(const std::string& msg, const Json::Value& param);
		void _write(const char* msg);
		void _write(const char* msg, const Json::Value& param);
	};
	void connect(uint32_t port);
}

