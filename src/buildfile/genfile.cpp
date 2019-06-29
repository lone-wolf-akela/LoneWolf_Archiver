#include "buildfile.h"

#include <cassert>

#include <fstream>
#include <iterator>
#include <tuple>

#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/adapt_adt.hpp>
#include <boost/spirit/include/support_adapt_adt_attributes.hpp>
#include <boost/spirit/include/karma.hpp>

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
	(obj.string(), std::ignore)
)

namespace buildfile
{	
	namespace karma = boost::spirit::karma;
	namespace stdw = boost::spirit::standard_wide;

	template <typename Iter>
	struct builfile_gen : karma::grammar<Iter, Archive()>
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
			(FileSettingCommand::Command::Override, "Override")
				(FileSettingCommand::Command::SkipFile, "SkipFile");
			filesetting_cmd_param = karma::eps <<
				"wildcard" << '=' << quoted_string << ' ' <<
				"minsize" << '=' << '"' << karma::int_ << '"' << ' ' <<
				"maxsize" << '=' << '"' << karma::int_ << '"' <<
				-(karma::eps << ' ' << "ct" << '=' << compression_sym);
			compression_sym.add
			(Compression(0), "\"0\"")
				(Compression(1), "\"1\"")
				(Compression(2), "\"2\"");

			quoted_string = '"' << *stdw::char_ << '"';
		}

		karma::symbols<FileSettingCommand::Command, const char*> command_sym;
		karma::symbols<Compression, const char*> compression_sym;

		karma::rule<Iter, std::u8string()> quoted_string;

		karma::rule<Iter, FileSettingCommand::Param()> filesetting_cmd_param;
		karma::rule<Iter, FileSettingCommand()> filesetting_command;
		karma::rule<Iter, FileSettings::Param()> filesettings_param;
		karma::rule<Iter, FileSettings()> filesettings;
		karma::rule<Iter, std::filesystem::path()> file;
		karma::rule<Iter, TOC::Param()> toc_param;
		karma::rule<Iter, TOC()> toc;
		karma::rule<Iter, Archive()> end;
	};

	void genFile(const std::filesystem::path& filepath, const Archive& archive)
	{
		std::ofstream ofile(filepath);

		assert(ofile);

		std::string generated;
		std::back_insert_iterator<std::string> iter(generated);
		buildfile::builfile_gen<decltype(iter)> gen;
		bool r = boost::spirit::karma::generate(iter, gen, archive);

		assert(r);

		ofile << generated;
	}
}