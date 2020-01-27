#include <optional>
#include <sstream>

#include <nlohmann/json.hpp>

#include "exceptions/exceptions.h"
#include "core.h"
#include "archive/archive.h"
#include "encoding/encoding.h"

#include "export.h"

namespace libexport
{
	struct Internal
	{
		std::optional<archive::Archive> file;
	};

	Interface::Interface() : _internal(new Internal){}

	Interface::~Interface() = default;

	void Interface::Open(std::wstring_view file) const
	{
		const std::filesystem::path filepath(file);
		_internal->file = archive::Archive();
		_internal->file->open(filepath, archive::Archive::Mode::Read);
	}

	void Interface::ExtractAll(
		std::wstring output_path,
		const ProgressCallback& callback
	) const
	{
		core::extract(*_internal->file, output_path, std::thread::hardware_concurrency(), callback);
	}

	void Interface::ExtractFile(
		std::wstring output_path,
		std::wstring file_path,
		const ProgressCallback& callback
	) const
	{
		throw NotImplementedError();
	}

	void Interface::ExtractFolder(
		std::wstring output_path,
		std::wstring folder_path,
		const ProgressCallback& callback
	) const
	{
		throw NotImplementedError();
	}

	void Interface::ExtractToc(
		std::wstring output_path,
		std::wstring toc,
		const ProgressCallback& callback
	) const
	{
		throw NotImplementedError();
	}

	std::string Interface::GetFiletree() const
	{
		return _internal->file->getFileTree().dump();
	}

	void Interface::Generate(
		std::wstring_view root,
		bool all_in_one,
		std::wstring_view archivepath,
		int thread_num,
		bool encryption,
		int compress_level,
		bool keep_sign,
		const std::vector<std::wstring>& ignore_list,
		uint_fast32_t seed
	)
	{
		std::vector<std::wstring> v_ignorelist;
		for (const auto& l : ignore_list)
		{
			v_ignorelist.emplace_back(l);
		}
		
		core::generate(
			root,
			all_in_one,
			archivepath,
			thread_num,
			encryption,
			compress_level,
			keep_sign,
			v_ignorelist,
			seed
		);
	}
}
