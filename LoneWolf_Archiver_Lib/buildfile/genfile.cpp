#include <cassert>

#include <fstream>
#include <iterator>
#include <tuple>

#pragma warning(push)
#pragma warning(disable : 4828)
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/adapt_adt.hpp>
#include <boost/spirit/include/support_adapt_adt_attributes.hpp>
#include <boost/spirit/include/karma.hpp>
#pragma warning(pop)

#include "buildfile.h"

BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand::Param, wildcard, minsize, maxsize, ct)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand, command, param)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings::Param, defcompression)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings, param, commands)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC::Param, name, alias, relativeroot)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC, param, filesetting, files)
BOOST_FUSION_ADAPT_STRUCT(buildfile::Archive, name, TOCs)

///\note	we need to output ANSI(std::string) instead of UTF8(std::u8string)
///			to keep compatible with Relic's old Archive.exe
BOOST_FUSION_ADAPT_ADT(
	std::filesystem::path,
	(absolute(obj).string(), std::ignore)
)

namespace
{
	namespace karma = boost::spirit::karma;

	template <typename Iter>
	struct builfile_gen : karma::grammar<Iter, buildfile::Archive()>
	{
		builfile_gen() : builfile_gen::base_type(end)
		{
			end = karma::eps <<
				"Archive" << ' ' <<
				"name" << '=' << quoted_string << karma::eol <<
				*toc;
			toc = karma::eps <<
				"TOCStart" << ' ' <<
				toc_param << karma::eol <<
				filesettings <<
				*file <<
				"TOCEnd" << karma::eol;
			toc_param = karma::eps <<
				"name" << '=' << quoted_string << ' ' <<
				"alias" << '=' << quoted_string << ' ' <<
				"relativeroot" << '=' << quoted_string;
			filesettings = karma::eps <<
				"FileSettingsStart" << ' ' <<
				filesettings_param << karma::eol <<
				*filesetting_command <<
				"FileSettingsEnd" << karma::eol;
			file = karma::string << karma::eol;
			filesettings_param = karma::eps <<
				"defcompression" << '=' << compression_sym;
			filesetting_command =
				command_sym << ' ' <<
				filesetting_cmd_param << karma::eol;
			command_sym.add
			(buildfile::FileSettingCommand::Command::Override, "Override")
				(buildfile::FileSettingCommand::Command::SkipFile, "SkipFile");
			filesetting_cmd_param = karma::eps <<
				"wildcard" << '=' << quoted_string << ' ' <<
				"minsize" << '=' << '"' << karma::int_ << '"' << ' ' <<
				"maxsize" << '=' << '"' << karma::int_ << '"' <<
				-(karma::eps << ' ' << "ct" << '=' << compression_sym);
			compression_sym.add
			(buildfile::Compression(0), "\"0\"")
				(buildfile::Compression(1), "\"1\"")
				(buildfile::Compression(2), "\"2\"");

			quoted_string = '"' << *karma::char_ << '"';
		}

		karma::symbols<buildfile::FileSettingCommand::Command, const char*> command_sym;
		karma::symbols<buildfile::Compression, const char*> compression_sym;

		karma::rule<Iter, std::u8string()> quoted_string;

		karma::rule<Iter, buildfile::FileSettingCommand::Param()> filesetting_cmd_param;
		karma::rule<Iter, buildfile::FileSettingCommand()> filesetting_command;
		karma::rule<Iter, buildfile::FileSettings::Param()> filesettings_param;
		karma::rule<Iter, buildfile::FileSettings()> filesettings;
		karma::rule<Iter, std::filesystem::path()> file;
		karma::rule<Iter, buildfile::TOC::Param()> toc_param;
		karma::rule<Iter, buildfile::TOC()> toc;
		karma::rule<Iter, buildfile::Archive()> end;
	};
}

namespace buildfile
{	
	void genFile(const std::filesystem::path& filepath, const Archive& archive)
	{
		create_directories(filepath.parent_path());
		std::ofstream ofile(filepath);

		if (!ofile)
		{
			throw FileIoError("Failed to open file: " +
				filepath.string() + " for output");
		}
		std::string generated;
		std::back_insert_iterator<std::string> iter(generated);
		const builfile_gen<decltype(iter)> gen;
		const bool r = generate(iter, gen, archive);

		if (!r) throw UnkownError("Unkown error happened when generating build file");

		ofile << generated;
	}
}
