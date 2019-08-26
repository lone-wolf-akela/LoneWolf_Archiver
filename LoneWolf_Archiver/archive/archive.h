#pragma once
#include <future>
#include <string_view>
#include <cstdint>

#include <json/json.h>

#include "../buildfile/buildfile.h"
#include "../ThreadPool/ThreadPool.h"
#include "../exceptions/exceptions.h"

namespace archive
{
	struct ArchiveInternal;
	class Archive
	{
	public:
		// initialization
		/// \param loggername the name of the spdlog logger
		explicit Archive(const std::string& loggername);
		Archive(const Archive&) = delete;
		Archive& operator=(const Archive&) = delete;
		Archive(Archive&& o) noexcept;
		Archive& operator=(Archive&& o) noexcept;
		enum class Mode { Read, Write_NonEncrypted, Write_Encrypted, Invalid};
		void open(const std::filesystem::path& filepath, Mode mode, uint_fast32_t encryption_key_seed = 0);

		// action
		std::future<void> extract(ThreadPool& pool, const std::filesystem::path& root);
		std::future<void> create(ThreadPool& pool,
			buildfile::Archive& task,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			const std::vector<std::u8string>& ignore_list);
		void listFiles() const;
		Json::Value getFileTree() const;
		std::future<bool> testArchive(ThreadPool& pool);
		std::u8string getArchiveSignature() const;

		// finish
		void close();
		~Archive();
	private:
		bool _opened = false;
		std::unique_ptr<ArchiveInternal> _internal;		
	};
}
