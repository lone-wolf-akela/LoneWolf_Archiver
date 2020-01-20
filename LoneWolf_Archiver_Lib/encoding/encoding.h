#if !defined(LONEWOLF_ARCHIVER_LIB_ENCODING_ENCODING_H)
#define LONEWOLF_ARCHIVER_LIB_ENCODING_ENCODING_H

#include <concepts>
#include <string>
#include <string_view>
#include <vector>
#include <type_traits>
#include <tuple>

#include <boost/locale.hpp>

#include "../exceptions/exceptions.h"

namespace encoding
{
	template<typename T>
	concept Char8 = std::same_as<T, char> ||
		std::same_as<T, signed char> ||
		std::same_as<T, unsigned char> ||
		std::same_as<T, char8_t>;
	

	template<Char8 CharTOut, Char8 CharTIn>
	std::basic_string<CharTOut> toUTF8(std::basic_string_view<CharTIn> in);
	template<Char8 CharT>
	std::u16string toUTF16(std::basic_string_view<CharT> in, std::locale codepage);
	template<Char8 CharT>
	std::basic_string<CharT> fromUTF16(std::u16string_view in, std::locale codepage);

	template<Char8 CharT>
	std::u16string toUTF16(std::basic_string_view<CharT> in, const char* codepage)
	{
		auto locale = boost::locale::generator().generate(codepage);
		return toUTF16(in, locale);
	}
	template<Char8 CharT>
	std::basic_string<CharT> fromUTF16(std::u16string in, const char* codepage)
	{
		auto locale = boost::locale::generator().generate(codepage);
		return fromUTF16<CharT>(in, locale);
	}
	
	template<Char8 CharTOut, Char8 CharTIn>
	std::basic_string<CharTOut>toUTF8(std::basic_string_view<CharTIn> in)
	{
		// try parse as utf8
		try
		{
			std::ignore = toUTF16(in, "utf8");
			// success
			return std::basic_string<CharTOut>(reinterpret_cast<const CharTOut*>(in.data()), in.length());
		}
		catch(boost::locale::conv::conversion_error&)
		{
			// not utf8, use system codepage
			auto system_locale = boost::locale::generator().generate("");
			auto u16 = toUTF16(in, system_locale);
			return fromUTF16<CharTOut>(u16, "utf8");
		}
	}

	template<Char8 CharT>
	std::basic_string<CharT> fromUTF16(std::u16string_view in, std::locale codepage)
	{
		const auto w = boost::locale::conv::utf_to_utf<wchar_t, char16_t>(in.data(), in.data() + in.length());
		if constexpr(std::is_same_v<CharT, char>)
		{
			return boost::locale::conv::from_utf(w, codepage);
		}
		else
		{
			const auto s = boost::locale::conv::from_utf(w, codepage);
			return std::basic_string<CharT>(reinterpret_cast<const CharT*>(s.c_str()), s.length());
		}
	}
	
	template<Char8 CharT>
	std::u16string toUTF16(std::basic_string_view<CharT> in, std::locale codepage)
	{
		const auto w = boost::locale::conv::to_utf<wchar_t>(
			reinterpret_cast<const char*>(in.data()),
			reinterpret_cast<const char*>(in.data() + in.length()),
			codepage
			);
		return boost::locale::conv::utf_to_utf<char16_t, wchar_t>(w);
	}
}

#endif
