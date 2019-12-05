#if !defined(LONEWOLF_ARCHIVER_LIB_BUILDFILE_BUILDFILE_H)
#define LONEWOLF_ARCHIVER_LIB_BUILDFILE_BUILDFILE_H

#include <cstdint>

#include <string>
#include <vector>
#include <list>
#include <filesystem>

#include <boost/optional/optional.hpp>

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
		enum class Command { Override, SkipFile } command = Command(0);
		struct Param
		{
			std::u8string wildcard = u8"";
			int64_t minsize = 0;
			int64_t maxsize = 0;
			boost::optional<Compression> ct = boost::none;
		}param = {};
	};

	struct FileSettings
	{
		struct Param
		{
			Compression defcompression = Compression(0);
		}param = {};
		std::vector<FileSettingCommand> commands = {};
	};

	struct TOC
	{
		struct Param
		{
			std::u8string name = u8"";
			std::u8string alias = u8"";
			std::u8string relativeroot = u8"";
		}param;
		FileSettings filesetting = {};
		std::list<std::filesystem::path> files = {};
	};

	struct Archive
	{
		std::u8string name = u8"";
		std::vector<TOC> TOCs = {};

		std::string filename = "";
	};


	Archive parseFile(const std::filesystem::path& filepath);
	void genFile(const std::filesystem::path& filepath, const Archive& archive);
	std::vector<Archive> scanFiles(const std::filesystem::path& rootpath, bool allinone);
}

#endif
