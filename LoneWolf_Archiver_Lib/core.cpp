#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "core.h"



namespace core
{
	archive::ProgressCallback makeLoggerCallback(const std::string& loggername)
	{
		auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		const auto logger = std::make_shared<spdlog::logger>(loggername, sink);
		return [logger](
			std::optional<std::string> msg,
			int current, int max,
			std::optional<std::string> filename,
			archive::MsgType type
			)
		{
			if(msg.has_value())
			{
				switch (type)
				{
				case archive::INFO:
					logger->info(*msg);
					break;
				case archive::WARN:
					logger->warn(*msg);
					break;
				default: /*case archive::ERR:*/
					logger->critical(*msg);
					break;
				}
			}
			if (filename.has_value())
			{
				const auto progress = double(current) * 100 / max;
				// only output progress when progress at least 1%
				if (lround(progress) != lround(double(current - 1) * 100 / max))
				{
					const auto complete = lround(progress * 0.3);
					const auto incomplete = size_t(30) - complete;
					logger->info("[{0}{1}] {2}%: {3}",
						std::string(complete, '#'),
						std::string(incomplete, '-'),
						lround(progress),
						filename);
				}
			}
		};
	}

	void generate(
		const std::filesystem::path& rootpath,
		bool allinone,
		const std::filesystem::path& archivepath,
		size_t threadNum,
		bool encryption,
		int compressLevel,
		bool keepSign,
		const std::vector<std::u8string>& ignoreList,
		uint_fast32_t encryption_key_seed,
		const archive::ProgressCallback& callback
	)
	{
		Timer t;
		auto tasks = buildfile::scanFiles(rootpath, allinone);
		ThreadPool pool(threadNum);
		std::vector<archive::Archive> files;
		std::vector<std::future<void>> futurelist;
		for (size_t i = 0; i < tasks.size(); i++)
		{
			files.emplace_back(tasks[i].filename);
			files.back().open(
				0 == i ?
				archivepath :
				archivepath.parent_path() / (tasks[i].filename + ".big"),
				encryption ?
				archive::Archive::Mode::Write_Encrypted :
				archive::Archive::Mode::Write_NonEncrypted,
				encryption ? encryption_key_seed : 0);
			futurelist.push_back(files.back().create(pool,
				tasks[i],
				rootpath,
				compressLevel,
				!keepSign,
				ignoreList,
				callback));
		}
		for (auto& f : futurelist)
		{
			f.get();
		}
	}

	void extract(
		archive::Archive& file,
		const std::filesystem::path& rootpath,
		size_t threadNum,
		const archive::ProgressCallback& callback
	)
	{
		Timer t;
		ThreadPool pool(threadNum);
		file.extract(pool, rootpath, callback).get();
	}
}
