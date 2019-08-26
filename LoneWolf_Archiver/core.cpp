#include "core.h"

namespace core
{
	void generate(
		const std::filesystem::path& rootpath,
		bool allinone,
		const std::filesystem::path& archivepath,
		size_t threadNum,
		bool encryption,
		int compressLevel,
		bool keepSign,
		const std::vector<std::u8string>& ignoreList,
		uint_fast32_t encryption_key_seed
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
				ignoreList));
		}
		for (auto& f : futurelist)
		{
			f.get();
		}
	}
}
