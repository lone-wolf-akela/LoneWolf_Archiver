#if !defined(LONEWOLF_ARCHIVER_LIB_ENCODING_ENCODING_H)
#define LONEWOLF_ARCHIVER_LIB_ENCODING_ENCODING_H

#include <concepts>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>
#include <tuple>

#include <boost/locale.hpp>

#include "../helper/helper.h"
#include "../exceptions/exceptions.h"

namespace encoding
{
	template<typename T>
	concept Char8 = std::same_as<T, char> ||
		std::same_as<T, signed char> ||
		std::same_as<T, unsigned char> ||
		std::same_as<T, char8_t>;

	template<Char8 CharT>
	std::basic_string<CharT> wide_to_narrow(std::wstring_view in, std::locale codepage)
	{
		if constexpr (std::is_same_v<CharT, char>)
		{
			return boost::locale::conv::from_utf(
				in.data(), in.data() + in.length(), codepage
			);
		}
		else
		{
			const auto s = boost::locale::conv::from_utf(
				in.data(), in.data() + in.length(), codepage
			);
			return std::basic_string<CharT>(pointer_cast<const CharT, char>(s.c_str()), s.length());
		}
	}
	template<Char8 CharT>
	std::basic_string<CharT> wide_to_narrow(std::wstring_view in, const char* codepage)
	{
		auto locale = boost::locale::generator().generate(codepage);
		return wide_to_narrow<CharT>(in, locale);
	}
	
	template<Char8 CharT>
	std::wstring narrow_to_wide(std::basic_string_view<CharT> in, std::locale codepage)
	{
		const auto w = boost::locale::conv::to_utf<wchar_t>(
			pointer_cast<const char, CharT>(in.data()),
			pointer_cast<const char, CharT>(in.data() + in.length()),
			codepage
			);
		return boost::locale::conv::utf_to_utf<wchar_t, wchar_t>(w);
	}

	template<Char8 CharT>
	std::wstring narrow_to_wide(std::basic_string_view<CharT> in, const char* codepage)
	{
		auto locale = boost::locale::generator().generate(codepage);
		return narrow_to_wide(in, locale);
	}

	template<Char8 CharT>
	std::wstring detect_narrow_to_wide(std::basic_string_view<CharT> in)
	{
		// try parse as utf8
		try
		{
			return narrow_to_wide(in, "utf8");
		}
		catch (boost::locale::conv::conversion_error&)
		{
			// not utf8, use system codepage
			return narrow_to_wide(in, "");
		}
	}
	
	template<Char8 CharTOut, Char8 CharTIn>
	std::basic_string<CharTOut> detect_narrow_to_utf8(std::basic_string_view<CharTIn> in)
	{
		// try parse as utf8
		try
		{
			std::ignore = narrow_to_wide(in, "utf8");
			// success
			return std::basic_string<CharTOut>(pointer_cast<const CharTOut, CharTIn>(in.data()), in.length());
		}
		catch(boost::locale::conv::conversion_error&)
		{
			// not utf8, use system codepage
			auto w = narrow_to_wide(in, "");
			return wide_to_narrow<CharTOut>(w, "utf8");
		}
	}

	std::u16string wide_to_utf16(std::wstring_view in);
	std::wstring utf16_to_wide(std::u16string_view in);
}

#endif
