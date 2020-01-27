#include <map>
#include <utility>

#include "buildfile.h"

namespace
{
	///\brief	returns the last element which is not empty in a path.
	///			returns an empty path if all element in the input path. 
	std::filesystem::path lastNotEmptyFilename(const std::filesystem::path& path)
	{
		for (auto iter = --path.end(); iter != path.begin(); --iter)
		{
			if (!iter->empty())
			{
				return *iter;
			}
		}
		return *path.begin();
	}
	bool pathHasPrefix(const std::filesystem::path& path, const std::filesystem::path& prefix)
	{
		const auto pair = std::mismatch(path.begin(), path.end(), prefix.begin(), prefix.end());
		return pair.second == prefix.end();
	}

	std::list<std::filesystem::path> findAllFiles(const std::filesystem::path& rootPath)
	{
		std::list<std::filesystem::path> filelist;
		for (std::filesystem::directory_iterator iter(rootPath);
			iter != std::filesystem::directory_iterator();
			++iter)
		{
			if (is_directory(*iter))
			{
				auto files_in_subdir = findAllFiles(*iter);
				filelist.splice(filelist.end(), files_in_subdir);
			}
			else
			{
				filelist.emplace_back(*iter);
			}
		}
		return filelist;
	}
}

namespace buildfile
{
	std::vector<Archive> scanFiles(const std::filesystem::path& rootpath, bool allinone)
	{
		FileSettings filesettings_template;
		filesettings_template.param.defcompression = Compression::Decompress_During_Read;
		// 1
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.*",
			.minsize = -1,
			.maxsize = 100,
			.ct = Compression::Uncompressed} });
		// 2
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.mp3",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 3
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.wav",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 4
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.jpg",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 5
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.lua",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 6
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.fda",
			.minsize = -1,
			.maxsize = -1,
			//.ct = Uncompressed} 
			///\note fda is not that bad to be compressed, so let's try this.
			.ct = Compression::Decompress_During_Read} });
		// 7
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.txt",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 8
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.ship",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 9
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.resource",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 10
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.pebble",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 11
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.level",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 12
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.wepn",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 13
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.subs",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 14
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.miss",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 15
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.events",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 16
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.madstate",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 17
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.script",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 18
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.ti",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 19
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.st",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 20
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.vp",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 21
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.wf",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 22
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.anim",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 23
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.mres",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 24
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.navs",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 25
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.mc",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 26
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.mtga",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 27
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.levels",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 28
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.campaign",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 29
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.flare",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 30
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.dustcloud",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 31
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.cloud",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 32
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.ahod",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 33
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = L"*.lod",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 34
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = L"Keeper.txt",
			.minsize = -1,
			.maxsize = -1,
			.ct = boost::none} });
		// 35
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = L"*.big",
			.minsize = -1,
			.maxsize = -1,
			.ct = boost::none} });
		// 36
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = L"_.*",
			.minsize = -1,
			.maxsize = -1,
			.ct = boost::none} });

		std::list<std::filesystem::path> allfiles = findAllFiles(rootpath);
		// find all languages in "locale" folder
		std::map<std::filesystem::path, std::list<std::filesystem::path>> locales;
		if (is_directory(rootpath / "locale"))
		{
			for (std::filesystem::directory_iterator locale_iter(rootpath / "locale");
				locale_iter != std::filesystem::directory_iterator();
				++locale_iter)
			{
				if (is_directory(*locale_iter))
				{
					std::list<std::filesystem::path> localefiles;
					//splice out all files in this folder from .allfiles
					for (auto file_iter = allfiles.begin(); file_iter != allfiles.end();)
					{
						if (pathHasPrefix(*file_iter, *locale_iter))
						{
							const auto tobe_sliced = file_iter;
							++file_iter;
							localefiles.splice(localefiles.end(), allfiles, tobe_sliced);
						}
						else
						{
							++file_iter;
						}
					}
					locales.emplace(lastNotEmptyFilename(locale_iter->path()), localefiles);
				}
			}
		}
		const std::string modname = lastNotEmptyFilename(rootpath).string();
		const std::wstring wmodname = lastNotEmptyFilename(rootpath).wstring();
		std::vector<Archive> archive_list;
		// build our base archive
		archive_list.emplace_back(Archive{ .name = L"MOD" + wmodname,.filename = modname });
		archive_list.back().TOCs.emplace_back(TOC{
			.param{
			.name = L"TOC" + wmodname,
			.alias = L"Data",
			.relativeroot = L""},
			.filesetting = filesettings_template,
			.files = std::move(allfiles) });
		// then build all the locale archive
		for (auto& [loc, locpath] : locales)
		{
			std::string locname = loc.string();
			std::wstring wlocname = loc.wstring();
			if (!allinone)
			{
				// create a new archive for this locale
				archive_list.emplace_back(Archive{
					.name = L"MOD" + wmodname + wlocname,.filename = locname });
			}
			archive_list.back().TOCs.emplace_back(TOC{
				.param{
				.name = L"TOC" + wmodname + wlocname,
				.alias = L"Locale",
				.relativeroot = L"locale\\" + wlocname },
				.filesetting = filesettings_template,
				.files = std::move(locpath) });
		}
		return archive_list;
	}
}
