#pragma once
#include <cstdint>

#include <string>
#include <vector>
#include <list>
#include <filesystem>

#include <boost/optional.hpp>

#include "../exceptions/exceptions.h"

namespace buildfile
{
	enum class Compression {
		Uncompressed = 0,
		Decompress_During_Read = 1,
		Decompress_All_At_Once = 2
	};

	struct FileSettingCommand
	{
		enum class Command { Override, SkipFile } command;
		struct Param
		{
			std::u8string wildcard;
			int64_t minsize;
			int64_t maxsize;
			boost::optional<Compression> ct;
		}param;
	};

	struct FileSettings
	{
		struct Param
		{
			Compression defcompression;
		}param = {};
		std::vector<FileSettingCommand> commands;
	};

	struct TOC
	{
		struct Param
		{
			std::u8string name;
			std::u8string alias;
			std::u8string relativeroot;
		}param;
		FileSettings filesetting;
		std::list<std::filesystem::path> files;
	};

	struct Archive
	{
		std::u8string name;
		std::vector<TOC> TOCs;

		std::string filename;
	};


	Archive parseFile(const std::filesystem::path& filepath);
	void genFile(const std::filesystem::path& filepath, const Archive& archive);
	std::vector<Archive> scanFiles(const std::filesystem::path& rootpath, bool allinone);
	std::string ConvertToUTF8(std::string_view in);
}
