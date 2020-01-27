#if !defined(LONEWOLF_ARCHIVER_LIB_EXPORT_H)
#define LONEWOLF_ARCHIVER_LIB_EXPORT_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <optional>

namespace libexport
{
	namespace defs
	{
		enum MsgType { INFO, WARN, ERR };
	}
	using namespace defs;
	using ProgressCallback = std::function<
		void(
			MsgType type,
			std::optional<std::string> msg,
			int current, int max,
			std::optional<std::string> filename
			)
	>;
	struct Internal;
	class Interface
	{
	public:
		Interface();
		///\note: we need this dtor to fully hide struct Internal into the .cpp file
		~Interface();
		Interface(const Interface&) = delete;
		Interface& operator=(const Interface&) = delete;
		Interface(Interface&&) = delete;
		Interface& operator=(Interface&&) = delete;
		
		void Open(std::wstring_view file) const;
		void ExtractAll(std::wstring output_path, const ProgressCallback& callback) const;
		void ExtractFile(std::wstring output_path, std::wstring file_path, const ProgressCallback& callback) const;
		void ExtractFolder(std::wstring output_path, std::wstring folder_path, const ProgressCallback& callback) const;
		void ExtractToc(std::wstring output_path, std::wstring toc, const ProgressCallback& callback) const;
		[[nodiscard]] std::string GetFiletree() const;
		void Generate(
			std::wstring_view root,
			bool all_in_one,
			std::wstring_view archivepath,
			int thread_num,
			bool encryption,
			int compress_level,
			bool keep_sign,
			const std::vector<std::wstring>& ignore_list,
			uint_fast32_t seed
		);
	private:
		std::unique_ptr<Internal> _internal;
	};
}

#endif
