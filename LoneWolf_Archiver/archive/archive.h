﻿#pragma once
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
		/// \param name the name of the spdlog logger
		Archive(const std::string& name);
		Archive(const Archive&) = delete;
		Archive& operator=(const Archive&) = delete;
		Archive(Archive&& o) noexcept;
		Archive& operator=(Archive&& o) noexcept;
		enum Mode { Read, Write_NonEncrypted, Write_Encrypted};
		void open(const std::filesystem::path& filepath, Mode mode);

		// action
		std::future<void> extract(ThreadPool& pool, const std::filesystem::path& root);
		std::future<void> create(ThreadPool& pool,
			buildfile::Archive& task,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			const std::vector<std::u8string>& ignore_list);
		void listFiles();
		std::future<bool> testArchive(ThreadPool& pool);
		std::u8string getArchiveSignature();

		// finish
		void close();
		~Archive();
	private:
		bool _opened;
		std::unique_ptr<ArchiveInternal> _internal;		
	};
}
