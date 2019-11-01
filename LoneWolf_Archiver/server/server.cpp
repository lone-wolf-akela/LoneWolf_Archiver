#include <cstddef>

#include <algorithm>
#include <istream>
#include <ostream>
#include <fstream>
#include <vector>
#include <array>

#include <json/writer.h>

#include "../helper/helper.h"
#include "server.h"

namespace server
{
	JsonServer::JsonServer() :
		_logger(std::make_shared<spdlog::logger>("server",
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>())),
		_acceptor(_io_context, tcp::endpoint(tcp::v4(), 0)),
		_socket(_io_context)
	{
		Json::StreamWriterBuilder builder;
		builder["commentStyle"] = "None";
		builder["indentation"] = "";
		_jsonwriter.reset(builder.newStreamWriter());
		
		const auto port = _acceptor.local_endpoint().port();
		{	
			std::ofstream portfile;
			portfile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
			portfile.open("archiver_server_port.tmp");
			portfile << port;
		}
		_logger->info("Server launched at port {0}.", port);
	}
	void JsonServer::start_listen()
	{
		{
			_acceptor.accept(_socket);
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
				_acceptor.close();
				_socket.close();
				_logger->info("connection closed.");

				return;
			}
			else if ("open" == msg_str)
			{
				_logger->info("open big file.");
				std::filesystem::path filepath(msg_json["param"]["path"].asString());
				_file = archive::Archive(filepath.filename().string());
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
					ignorelist,
					param["seed"].asUInt());
				_write("ok");
			}
			else
			{
				throw ServerError("Unknown client message: " + msg_str);
			}
		}
	}
	Json::Value JsonServer::_read()
	{
		std::array<std::byte, sizeof(int32_t)> lenarray;	
		boost::asio::read(_socket, boost::asio::buffer(lenarray));
		const auto len = FromBigEndian<int32_t>(lenarray);
		
		boost::asio::streambuf strmbuf;
		boost::asio::read(_socket, strmbuf, boost::asio::transfer_exactly(len));
		
		std::istream strm(&strmbuf);
		Json::Value msg;
		strm >> msg;
		return msg;
	}
	void JsonServer::_write(const Json::Value& msg)
	{
		boost::asio::streambuf strmbuf;
		std::ostream strm(&strmbuf);
		_jsonwriter->write(msg, &strm);
		boost::asio::write(_socket, boost::asio::buffer(ToBigEndian(int32_t(strmbuf.size()))));
		boost::asio::write(_socket, strmbuf, boost::asio::transfer_all());
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
	void start()
	{
		JsonServer s;
		s.start_listen();
	}
}

