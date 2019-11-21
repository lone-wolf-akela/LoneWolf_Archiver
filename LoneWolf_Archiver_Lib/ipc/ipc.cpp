#include <cstddef>

#include <algorithm>
#include <istream>
#include <ostream>
#include <fstream>
#include <vector>
#include <array>

#include <json/writer.h>
#include <boost/endian/conversion.hpp>

#include "ipc.h"

namespace ipc
{
	IpcWorker::IpcWorker() :
		_logger(std::make_shared<spdlog::logger>("ipc",
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>())),
		_socket(_io_context)
	{
		Json::StreamWriterBuilder builder;
		builder["commentStyle"] = "None";
		builder["indentation"] = "";
		_jsonwriter.reset(builder.newStreamWriter());
	}
	void IpcWorker::connect(uint32_t port)
	{
		{
			const auto endpoint =
				tcp::endpoint(boost::asio::ip::address::from_string("127.0.0.1"), port);
			_socket.connect(endpoint);
			Json::Value msg_read = _read();
			const auto msg_str = msg_read["msg"].asString();
			if ("hello" == msg_str)
			{
				_write("hello");
				_logger->info("connected to port {0}.", port);
			}
			else
			{
				throw IpcError("Unknown ipc message: " + msg_str);
			}
		}
		while (true)
		{
			Json::Value msg_json = _read();
			auto msg_str = msg_json["msg"].asString();
			if ("bye" == msg_str)
			{
				_write("bye");
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
				throw IpcError("Unknown client message: " + msg_str);
			}
		}
	}
	Json::Value IpcWorker::_read()
	{
		int32_t len;	
		boost::asio::read(_socket, boost::asio::buffer(&len, sizeof(len)));
		boost::endian::big_to_native_inplace(len);
		
		boost::asio::streambuf strmbuf;
		boost::asio::read(_socket, strmbuf, boost::asio::transfer_exactly(len));
		
		std::istream strm(&strmbuf);
		Json::Value msg;
		strm >> msg;
		return msg;
	}
	void IpcWorker::_write(const Json::Value& msg)
	{
		boost::asio::streambuf strmbuf;
		std::ostream strm(&strmbuf);
		_jsonwriter->write(msg, &strm);
		auto len = boost::endian::native_to_big(int32_t(strmbuf.size()));
		boost::asio::write(_socket, boost::asio::buffer(&len, sizeof(len)));
		boost::asio::write(_socket, strmbuf, boost::asio::transfer_all());
	}
	void IpcWorker::_write(const std::string& msg)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		_write(root);
	}
	void IpcWorker::_write(const std::string& msg, const Json::Value& param)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		root["param"] = param;
		_write(root);
	}
	void IpcWorker::_write(const char* msg)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		_write(root);
	}
	void IpcWorker::_write(const char* msg, const Json::Value& param)
	{
		Json::Value root(Json::objectValue);
		root["msg"] = msg;
		root["param"] = param;
		_write(root);
	}
	void connect(uint32_t port)
	{
		IpcWorker s;
		s.connect(port);
	}
}

