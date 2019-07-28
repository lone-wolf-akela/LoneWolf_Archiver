#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <iostream>

#include "archive/archive.h"

namespace core
{
	class Timer
	{
	public:
		Timer() :start(std::chrono::system_clock::now()) {}
		~Timer()
		{
			const auto end = std::chrono::system_clock::now();
			std::chrono::duration<double, std::ratio<1>> diff = end - start;
			std::cout << "Operation took " << std::fixed << std::setprecision(2)
				<< diff.count() << " seconds." << std::endl;
		}
	private:
		decltype(std::chrono::system_clock::now()) start;
	};
	void generate(
		const std::filesystem::path& rootpath,
		bool allinone,
		const std::filesystem::path& archivepath,
		size_t threadNum,
		bool encryption,
		int compressLevel,
		bool keepSign,
		const std::vector<std::u8string>& ignoreList
	);
}
