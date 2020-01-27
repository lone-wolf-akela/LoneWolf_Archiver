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

#include "../encoding/encoding.h"
#include "buildfile.h"

BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand::Param, wildcard, minsize, maxsize, ct)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand, command, param)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings::Param, defcompression)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings, param, commands)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC::Param, name, alias, relativeroot)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC, param, filesetting, files)
BOOST_FUSION_ADAPT_STRUCT(buildfile::Archive, name, TOCs)

BOOST_FUSION_ADAPT_ADT(
	std::filesystem::path,
	(absolute(obj).wstring(), std::ignore)
)

namespace
{
	namespace karma = boost::spirit::karma;
	namespace stdw = karma::standard_wide;
	
	template <typename Iter>
	struct builfile_gen : karma::grammar<Iter, buildfile::Archive()>
	{
		builfile_gen() : builfile_gen::base_type(end)
		{
			end = karma::eps <<
				L"Archive" << L' ' <<
				L"name" << L'=' << quoted_string << karma::eol <<
				*toc;
			toc = karma::eps <<
				L"TOCStart" << L' ' <<
				toc_param << karma::eol <<
				filesettings <<
				*file <<
				L"TOCEnd" << karma::eol;
			toc_param = karma::eps <<
				L"name" << L'=' << quoted_string << L' ' <<
				L"alias" << L'=' << quoted_string << L' ' <<
				L"relativeroot" << L'=' << quoted_string;
			filesettings = karma::eps <<
				L"FileSettingsStart" << L' ' <<
				filesettings_param << karma::eol <<
				*filesetting_command <<
				L"FileSettingsEnd" << karma::eol;
			file = stdw::string << karma::eol;
			filesettings_param = karma::eps <<
				L"defcompression" << L'=' << compression_sym;
			filesetting_command =
				command_sym << L' ' <<
				filesetting_cmd_param << karma::eol;
			command_sym.add
			(buildfile::FileSettingCommand::Command::Override, L"Override")
				(buildfile::FileSettingCommand::Command::SkipFile, L"SkipFile");
			filesetting_cmd_param = karma::eps <<
				L"wildcard" << L'=' << quoted_string << L' ' <<
				L"minsize" << L'=' << L'"' << karma::int_ << L'"' << L' ' <<
				L"maxsize" << L'=' << L'"' << karma::int_ << L'"' <<
				-(karma::eps << L' ' << L"ct" << L'=' << compression_sym);
			compression_sym.add
			(buildfile::Compression(0), L"\"0\"")
				(buildfile::Compression(1), L"\"1\"")
				(buildfile::Compression(2), L"\"2\"");

			quoted_string = L'"' << *stdw::char_ << L'"';
		}

		karma::symbols<buildfile::FileSettingCommand::Command, const wchar_t*> command_sym;
		karma::symbols<buildfile::Compression, const wchar_t*> compression_sym;

		karma::rule<Iter, std::wstring()> quoted_string;

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
		std::wstring generated;
		std::back_insert_iterator<std::wstring> iter(generated);
		const builfile_gen<decltype(iter)> gen;
		const bool r = generate(iter, gen, archive);

		if (!r) throw UnkownError("Unkown error happened when generating build file");

		///\note	we need to output ANSI(std::string) instead of UTF8(std::u8string)
///			to keep compatible with Relic's old Archive.exe
		ofile << encoding::wide_to_narrow<char>(generated, "");
	}
}
