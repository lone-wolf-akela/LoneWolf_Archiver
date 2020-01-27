#if !defined(LONEWOLF_ARCHIVER_LIB_CORE_H)
#define LONEWOLF_ARCHIVER_LIB_CORE_H

#include <cstdint>
#include <filesystem>
#include <iostream>

#include "archive/archive.h"

namespace core
{
	class Timer
	{
	public:
		Timer() :start(std::chrono::system_clock::now()) {}
		Timer(const Timer&) = delete;
		Timer& operator=(const Timer&) = delete;
		Timer(Timer&&) = delete;
		Timer& operator=(Timer&&) = delete;
		~Timer()
		{
			const auto end = std::chrono::system_clock::now();
			const std::chrono::duration<double, std::ratio<1>> diff = end - start;
			std::cout << "Operation took " << std::fixed << std::setprecision(2)
				<< diff.count() << " seconds." << std::endl;
		}
	private:
		decltype(std::chrono::system_clock::now()) start = {};
	};
	archive::ProgressCallback makeLoggerCallback(const std::string& loggername);
	void generate(
		const std::filesystem::path& rootpath,
		bool allinone,
		const std::filesystem::path& archivepath,
		size_t threadNum,
		bool encryption,
		int compressLevel,
		bool keepSign,
		const std::vector<std::wstring>& ignoreList,
		uint_fast32_t encryption_key_seed,
		std::optional<archive::ProgressCallback> callback = std::nullopt
	);
	void extract(
		archive::Archive& file,
		const std::filesystem::path& rootpath,
		size_t threadNum,
		std::optional<archive::ProgressCallback> callback = std::nullopt
	);
}

#endif
