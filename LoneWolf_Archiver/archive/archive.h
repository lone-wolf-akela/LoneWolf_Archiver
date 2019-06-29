#pragma once
#include "../buildfile/buildfile.h"
#include "../ThreadPool/ThreadPool.h"

namespace archive
{
	struct ArchiveInternal;
	class Archive
	{
	public:
		// initialization
		Archive();
		enum Mode { Read, Write };
		void open(const std::filesystem::path& filepath, Mode mode, bool encryption = false);

		// action
		void extract(ThreadPool& pool, const std::filesystem::path& directory);
		void create(ThreadPool& pool,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			std::vector<std::string> ignore_list);
		void listFiles();
		void testArchive();
		std::string getArchiveSignature();

		// finish
		void close();
		~Archive() { close(); }
	private:
		std::unique_ptr<ArchiveInternal> _internal;
	};
}
