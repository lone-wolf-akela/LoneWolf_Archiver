﻿#include <cassert>
#include <cstring>

#include <fstream>
#include <sstream>

#pragma warning(push)
#pragma warning(disable : 4828)
#include <boost/spirit/home/x3.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#pragma warning(pop)

#include "../encoding/encoding.h"
#include "buildfile.h"

BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettingCommand, command, param)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings::Param, defcompression)
BOOST_FUSION_ADAPT_STRUCT(buildfile::FileSettings, param, commands)


BOOST_FUSION_ADAPT_STRUCT(buildfile::TOC, param, filesetting, files)
BOOST_FUSION_ADAPT_STRUCT(buildfile::Archive, name, TOCs)

namespace
{
	namespace x3 = boost::spirit::x3;
	namespace stdw = x3::standard_wide;
	using boost::fusion::operator<<;
	
	const x3::rule<class bf_skipper> bf_skipper = "bf_skipper";
	const auto bf_skipper_def =
		(x3::eol >> *stdw::space >> L"//" >> *(stdw::char_ - x3::eol)) | stdw::space;
	BOOST_SPIRIT_DEFINE(bf_skipper);
	
	struct command_sym_ : stdw::symbols<buildfile::FileSettingCommand::Command>
	{
		command_sym_()
		{
			add(L"Override", buildfile::FileSettingCommand::Command::Override)
				(L"SkipFile", buildfile::FileSettingCommand::Command::SkipFile);
		}
	} command_sym;
	struct compression_sym_ : stdw::symbols<buildfile::Compression>
	{
		compression_sym_()
		{
			add
			(L"\"0\"", buildfile::Compression(0))
				(L"\"1\"", buildfile::Compression(1))
				(L"\"2\"", buildfile::Compression(2));
		}
	} compression_sym;

	const x3::rule<class quoted_string, std::wstring> quoted_string = "quoted_string";
	const auto quoted_string_def = x3::lexeme[L'"' > *(stdw::char_ - L'"') > L'"'];
	BOOST_SPIRIT_DEFINE(quoted_string);

	const x3::rule<class line, std::wstring> line = "line";
	const auto line_def = x3::lexeme[*(stdw::char_ - x3::eol) > &x3::eol];
	BOOST_SPIRIT_DEFINE(line);

	const auto quoted_int64 = x3::lit(L"\"") > x3::int64 > L'"';
	
	const x3::rule<class toc_param, buildfile::TOC::Param> toc_param = "toc_param";
	constexpr auto toc_param_name = [&](auto& ctx) {x3::_val(ctx).name = x3::_attr(ctx); };
	constexpr auto toc_param_alias = [&](auto& ctx) {x3::_val(ctx).alias = x3::_attr(ctx); };
	constexpr auto toc_param_relativeroot = [&](auto& ctx) {x3::_val(ctx).relativeroot = x3::_attr(ctx); };
	const auto toc_param_def = +(
		(x3::lit(L"name") > L'=' > quoted_string[toc_param_name]) |
		(x3::lit(L"alias") > L'=' > quoted_string[toc_param_alias]) |
		(x3::lit(L"relativeroot") > L'=' > quoted_string[toc_param_relativeroot])
		);
	BOOST_SPIRIT_DEFINE(toc_param);

	const x3::rule<class filesetting_cmd_param, buildfile::FileSettingCommand::Param>
		filesetting_cmd_param = "filesetting_cmd_param";
	constexpr auto filesetting_cmd_param_wildcard = [&](auto& ctx) {x3::_val(ctx).wildcard = x3::_attr(ctx); };
	constexpr auto filesetting_cmd_param_minsize = [&](auto& ctx) {x3::_val(ctx).minsize = x3::_attr(ctx); };
	constexpr auto filesetting_cmd_param_maxsize = [&](auto& ctx) {x3::_val(ctx).maxsize = x3::_attr(ctx); };
	constexpr auto filesetting_cmd_param_ct = [&](auto& ctx) {x3::_val(ctx).ct = x3::_attr(ctx); };
	const auto filesetting_cmd_param_def = +(
		(x3::lit(L"wildcard") > L'=' > quoted_string[filesetting_cmd_param_wildcard]) |
		(x3::lit(L"minsize") > L'=' > quoted_int64[filesetting_cmd_param_minsize]) |
		(x3::lit(L"maxsize") > L'=' > quoted_int64[filesetting_cmd_param_maxsize]) |
		(x3::lit(L"ct") > L'=' > compression_sym[filesetting_cmd_param_ct])
		);
	BOOST_SPIRIT_DEFINE(filesetting_cmd_param);
	
	const x3::rule<class builfile_parser, buildfile::Archive> builfile_parser = "builfile_parser";
	const auto filesettings_param =
		x3::lit(L"defcompression") > L'=' > compression_sym;
	const auto filesetting_command =
		command_sym >
		filesetting_cmd_param;

	const x3::rule<class filesettings, buildfile::FileSettings> filesettings = "filesettings";
	const auto filesettings_def = 
		x3::lit(L"FileSettingsStart") >
		filesettings_param >
		*filesetting_command >
		L"FileSettingsEnd";
	BOOST_SPIRIT_DEFINE(filesettings);

	const x3::rule<class file, std::filesystem::path> file = "file";
	const auto file_def = !x3::lit(L"TOCEnd") > line;
	BOOST_SPIRIT_DEFINE(file);
	
	const x3::rule<class toc, buildfile::TOC> toc = "toc";
	const auto toc_def = 
		x3::lit(L"TOCStart") >
		toc_param >
		filesettings >
		*file >
		L"TOCEnd";
	BOOST_SPIRIT_DEFINE(toc);
	
	const auto builfile_parser_def = x3::eps >>
		L"Archive" >
		(x3::lit(L"name") > L'=' > quoted_string) >
		*toc >
		x3::eoi;
	BOOST_SPIRIT_DEFINE(builfile_parser);
}

namespace buildfile
{
	Archive parseFile(const std::filesystem::path& filepath)
	{
		std::ifstream ifile(filepath);
		if (!ifile) 
		{
			throw FileIoError("Failed to open file: " +
				filepath.string() + " for input");
		}
		std::stringstream buffer;
		buffer << ifile.rdbuf();
		
		std::wstring bufstr = encoding::detect_narrow_to_wide<char>(buffer.str());
		std::wstring::iterator iter, eof;
		// check & skip BOM head
		// Also add a '\n', which is a hack to skip possible comment in the first line
		if (L'\uFEFF' == bufstr[0])
		{
			//has BOM
			bufstr[0] = L'\n';
		}
		else
		{
			//no BOM
			bufstr = L'\n' + bufstr;
		}
		iter = bufstr.begin();
		eof = bufstr.end();
		
		Archive archive{ .filename = filepath.filename().string() };
		
		bool r = phrase_parse(iter, eof, builfile_parser, bf_skipper, archive);
		
		if (!(r && iter == eof)) throw FormatError("Cannot parse input build file");

		return archive;
	}
}
