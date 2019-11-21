#pragma once
#include <future>
#include <string_view>
#include <cstdint>
#include <functional>
#include <optional>

#include <json/json.h>

#include "../buildfile/buildfile.h"
#include "../ThreadPool/ThreadPool.h"
#include "../exceptions/exceptions.h"

namespace archive
{
	enum MsgType : uint8_t { INFO = 0, WARN = 1, ERR = 2 };
	using ProgressCallback = std::function<
		void(
			std::optional<std::string> msg,
			int current, int max,
			std::optional<std::string> filename,
			MsgType type
			)
	>;
	
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
		std::future<void> extract(
			ThreadPool& pool,
			const std::filesystem::path& root,
			const ProgressCallback& callback
		);
		std::future<void> create(ThreadPool& pool,
			buildfile::Archive& task,
			const std::filesystem::path& root,
			int compress_level,
			bool skip_tool_signature,
			const std::vector<std::u8string>& ignore_list,
			const ProgressCallback& callback
		);
		void listFiles() const;
		Json::Value getFileTree() const;
		std::future<bool> testArchive(ThreadPool& pool, const ProgressCallback& callback);
		std::u8string getArchiveSignature() const;

		// finish
		void close();
		~Archive();
	private:
		bool _opened = false;
		std::unique_ptr<ArchiveInternal> _internal;		
	};
}
