#pragma once
#include <future>

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
		enum Mode { Read, Write_NonEncrypted, Write_Encrypted};
		void open(const std::filesystem::path& filepath, Mode mode);

		// action
		std::future<void> extract(ThreadPool& pool, const std::filesystem::path& root);
		std::future<void> create(ThreadPool& pool,
			const buildfile::Archive& task,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			const std::vector<std::u8string>& ignore_list);
		void listFiles();
		std::future<bool> testArchive(ThreadPool& pool);
		std::u8string getArchiveSignature();

		// finish
		void close();
		~Archive() { close(); }
	private:
		std::unique_ptr<ArchiveInternal> _internal;		
	};
}
