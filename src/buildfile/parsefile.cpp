#include "buildfile.h"

#include <cassert>
#include <cstring>

#include <fstream>
#include <sstream>
#include <iterator>
#include <array>

#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/repository/include/qi_kwd.hpp>
#include <boost/spirit/repository/include/qi_keywords.hpp>

BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand::Param, wildcard, minsize, maxsize, ct)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand, command, param)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings::Param, defcompression)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings, param, commands)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC::Param, name, alias, relativeroot)
BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC, param, filesetting, files)
BOOST_FUSION_ADAPT_STRUCT(buildfile::Archive, name, TOCs)
namespace buildfile
{
	namespace qi = boost::spirit::qi;
	namespace repo_qi = boost::spirit::repository::qi;
	namespace stdw = boost::spirit::standard_wide;

	template<typename Iter>
	struct builfile_skipper : public qi::grammar<Iter>
	{
		builfile_skipper() : builfile_skipper::base_type(skip)
		{
			skip =  (qi::eol >> *stdw::space >> "//" >> *(qi::char_ - qi::eol)) | stdw::space;
		}
		qi::rule<Iter> skip;
	};

	template <typename Iter>
	struct builfile_parser : qi::grammar<Iter, Archive(), builfile_skipper<Iter>>
	{
		builfile_parser() : builfile_parser::base_type(start)
		{
			start %= qi::eps >>
				"Archive" >>
				"name" >> '=' > quoted_string >>
				*toc >>
				qi::eoi;
			toc %= qi::eps >>
				"TOCStart" >>
				toc_param >>
				filesettings >>
				*file >>
				"TOCEnd";
			toc_param %=
				repo_qi::kwd("name", 1)['=' > quoted_string] /
				repo_qi::kwd("alias", 1)['=' > quoted_string] /
				repo_qi::kwd("relativeroot", 1)['=' > quoted_string];
			filesettings %= qi::eps >>
				"FileSettingsStart" >>
				filesettings_param >>
				*filesetting_command >>
				"FileSettingsEnd";
			file %= !qi::lit("TOCEnd") >> line;
			filesettings_param %=
				repo_qi::kwd("defcompression", 1)['=' > compression_sym];
			filesetting_command %=
				command_sym >>
				filesetting_cmd_param;
			command_sym.add
			("Override", FileSettingCommand::Command::Override)
				("SkipFile", FileSettingCommand::Command::SkipFile);
			filesetting_cmd_param %=
				repo_qi::kwd("wildcard", 1)['=' > quoted_string] /
				repo_qi::kwd("minsize", 1)[qi::eps > '=' > '"' > qi::int_ > '"'] /
				repo_qi::kwd("maxsize", 1)[qi::eps > '=' > '"' > qi::int_ > '"'] /
				repo_qi::kwd("ct", 0, 1)['=' > compression_sym];
			compression_sym.add
			("\"0\"", Compression(0))
				("\"1\"", Compression(1))
				("\"2\"", Compression(2));

			line %= qi::lexeme[*(stdw::char_ - qi::eol) >> &qi::eol];
			quoted_string %= qi::lexeme['"' >> *(stdw::char_ - '"') >> '"'];
		}
		qi::symbols<char, FileSettingCommand::Command> command_sym;
		qi::symbols<char, Compression> compression_sym;

		qi::rule<Iter, std::u8string(), builfile_skipper<Iter>> line;
		qi::rule<Iter, std::u8string(), builfile_skipper<Iter>> quoted_string;

		qi::rule<Iter, FileSettingCommand::Param(), builfile_skipper<Iter>> filesetting_cmd_param;
		qi::rule<Iter, FileSettingCommand(), builfile_skipper<Iter>> filesetting_command;
		qi::rule<Iter, FileSettings::Param(), builfile_skipper<Iter>> filesettings_param;
		qi::rule<Iter, FileSettings(), builfile_skipper<Iter>> filesettings;
		qi::rule<Iter, std::filesystem::path(), builfile_skipper<Iter>> file;
		qi::rule<Iter, TOC::Param(), builfile_skipper<Iter>> toc_param;
		qi::rule<Iter, TOC(), builfile_skipper<Iter>> toc;
		qi::rule<Iter, Archive(), builfile_skipper<Iter>> start;
	};

	Archive parseFile(const std::filesystem::path& filepath)
	{
		std::ifstream ifile(filepath);
		
		assert(ifile);
		std::stringstream buffer;

		buffer << ifile.rdbuf();
		
		std::string bufstr = ConvertToUTF8(buffer.str());
		std::string::iterator iter, eof;
		// check & skip BOM head
		// Also add a '\n', which is a hack to skip possible comment in the first line
		if (0 == strncmp(bufstr.c_str(), "\xef\xbb\xbf", 3))
		{
			//has BOM
			bufstr[2] = '\n';
			iter = bufstr.begin() + 2;
			eof = bufstr.end();
		}
		else
		{
			//no BOM
			bufstr = '\n' + bufstr;
			iter = bufstr.begin();
			eof = bufstr.end();
		}
		
		buildfile::Archive archive{ .filename = filepath.filename().u8string() };
		buildfile::builfile_parser<decltype(iter)> parser;
		buildfile::builfile_skipper<decltype(iter)> skipper;
		bool r = boost::spirit::qi::phrase_parse(iter, eof, parser, skipper, archive);
		
		assert(r && iter == eof);

		return archive;
	}
}