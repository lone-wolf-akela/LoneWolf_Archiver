﻿#pragma once
#include <future>
#include <string_view>
#include <cstdint>
#include <functional>

#include <json/json.h>

#include "../export.h"
#include "../buildfile/buildfile.h"
#include "../ThreadPool/ThreadPool.h"
#include "../exceptions/exceptions.h"

namespace archive
{
	using namespace libexport::defs;
	using libexport::ProgressCallback;
	
	struct ArchiveInternal;
	class Archive
	{
	public:
		Archive();
		Archive(const Archive&) = delete;
		Archive& operator=(const Archive&) = delete;
		Archive(Archive&& o) noexcept;
		Archive& operator=(Archive&& o) noexcept;
		enum class Mode { Read, Write_NonEncrypted, Write_Encrypted, Invalid};
		void open(
			const std::filesystem::path& filepath,
			Mode mode,
			uint_fast32_t encryption_key_seed = 0
		);

		// action
		[[nodiscard]] std::future<void> extract(
			ThreadPool& pool,
			const std::filesystem::path& root,
			const ProgressCallback& callback
		);
		[[nodiscard]] std::future<void> create(ThreadPool& pool,
			buildfile::Archive& task,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			const std::vector<std::u8string>& ignore_list,
			const ProgressCallback& callback
		);
		void listFiles() const;
		[[nodiscard]] Json::Value getFileTree() const;
		[[nodiscard]] std::future<bool> testArchive(ThreadPool& pool, const ProgressCallback& callback);
		[[nodiscard]] std::u8string getArchiveSignature() const;

		// finish
		void close();
		~Archive();
	private:
		bool _opened;
		std::unique_ptr<ArchiveInternal> _internal;		
	};
}
