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
			.wildcard = u8"*.*",
			.minsize = -1,
			.maxsize = 100,
			.ct = Compression::Uncompressed} });
		// 2
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.mp3",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 3
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.wav",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 4
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.jpg",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Uncompressed} });
		// 5
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.lua",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 6
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.fda",
			.minsize = -1,
			.maxsize = -1,
			//.ct = Uncompressed} 
			///\note fda is not that bad to be compressed, so let's try this.
			.ct = Compression::Decompress_During_Read} });
		// 7
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.txt",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 8
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.ship",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 9
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.resource",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 10
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.pebble",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 11
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.level",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 12
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.wepn",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 13
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.subs",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 14
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.miss",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 15
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.events",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 16
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.madstate",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 17
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.script",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 18
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.ti",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 19
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.st",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 20
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.vp",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 21
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.wf",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 22
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.anim",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 23
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.mres",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 24
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.navs",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 25
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.mc",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 26
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.mtga",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 27
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.levels",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 28
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.campaign",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 29
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.flare",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 30
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.dustcloud",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 31
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.cloud",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 32
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.ahod",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 33
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::Override ,
			.param{
			.wildcard = u8"*.lod",
			.minsize = -1,
			.maxsize = -1,
			.ct = Compression::Decompress_All_At_Once} });
		// 34
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = u8"Keeper.txt",
			.minsize = -1,
			.maxsize = -1,
			.ct = boost::none} });
		// 35
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = u8"*.big",
			.minsize = -1,
			.maxsize = -1,
			.ct = boost::none} });
		// 36
		filesettings_template.commands.emplace_back(FileSettingCommand{
			.command = FileSettingCommand::Command::SkipFile ,
			.param{
			.wildcard = u8"_.*",
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
		const std::u8string u8modname = lastNotEmptyFilename(rootpath).u8string();
		std::vector<Archive> archive_list;
		// build our base archive
		archive_list.emplace_back(Archive{ .name = u8"MOD" + u8modname,.filename = modname });
		archive_list.back().TOCs.emplace_back(TOC{
			.param{
			.name = u8"TOC" + u8modname,
			.alias = u8"Data",
			.relativeroot = u8""},
			.filesetting = filesettings_template,
			.files = std::move(allfiles) });
		// then build all the locale archive
		for (auto& [loc, locpath] : locales)
		{
			std::string locname = loc.string();
			std::u8string u8locname = loc.u8string();
			if (!allinone)
			{
				// create a new archive for this locale
				archive_list.emplace_back(Archive{
					.name = u8"MOD" + u8modname + u8locname,.filename = locname });
			}
			archive_list.back().TOCs.emplace_back(TOC{
				.param{
				.name = u8"TOC" + u8modname + u8locname,
				.alias = u8"Locale",
				.relativeroot = u8"locale\\" + u8locname },
				.filesetting = filesettings_template,
				.files = std::move(locpath) });
		}
		return archive_list;
	}
}
