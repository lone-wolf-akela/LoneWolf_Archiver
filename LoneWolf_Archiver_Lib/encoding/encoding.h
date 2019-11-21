﻿#pragma once
#include <concepts>
#include <string>
#include <string_view>
#include <vector>

#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>

#include "../exceptions/exceptions.h"

namespace encoding
{
	template<typename T>
	concept Char8 = std::same_as<T, char> ||
		std::same_as<T, signed char> ||
		std::same_as<T, unsigned char> ||
		std::same_as<T, char8_t>;
	///\brief	Convert string to utf8.
	template<Char8 CharTOut, Char8 CharTIn>
	std::basic_string<CharTOut> toUTF8(std::basic_string_view<CharTIn> in);
	
	template<Char8 CharT>
	std::u16string toUTF16(std::basic_string_view<CharT> in, const char* codepage);
	template<Char8 CharT>
	std::basic_string<CharT> fromUTF16(std::u16string in, const char* codepage);
}

namespace encoding
{
	using namespace std::literals;

	inline void icuCheckFailure(UErrorCode status)
	{
		if (U_FAILURE(status))
		{
			throw EncodingError("Error: "s + u_errorName(status));
		}
	}

	class EncodingDetector
	{
	public:
		EncodingDetector() :
			_status(U_ZERO_ERROR),
			_ptr(ucsdet_open(&_status))
		{
			icuCheckFailure(_status);
		}

		std::vector<std::string> detectAll(std::string_view in)
		{
			ucsdet_setText(_ptr.getAlias(), in.data(), int32_t(in.size()), &_status);
			icuCheckFailure(_status);

			int32_t matches_num;
			const auto matches = ucsdet_detectAll(_ptr.getAlias(), &matches_num, &_status);
			icuCheckFailure(_status);

			std::vector<std::string> result;
			result.reserve(matches_num);
			for (int32_t i = 0; i < matches_num; i++)
			{
				result.emplace_back(ucsdet_getName(matches[i], &_status));
				icuCheckFailure(_status);
			}
			return result;
		}
	private:
		UErrorCode _status;
		icu::LocalUCharsetDetectorPointer _ptr;
	};

	class EncodingConverter
	{
	public:
		EncodingConverter(const char* codepage) :
			_status(U_ZERO_ERROR),
			_ptr(ucnv_open(codepage, &_status))
		{
			icuCheckFailure(_status);
		}
		template<Char8 CharT>
		std::basic_string<CharT> fromUChars(std::u16string_view in)
		{
			const auto len = ucnv_fromUChars(_ptr.getAlias(),
				nullptr, 0,
				in.data(), int32_t(in.size()),
				&_status);
			icuCheckFailure(_status);
			std::basic_string<CharT> out(size_t(len), 0);
			ucnv_fromUChars(_ptr.getAlias(),
				reinterpret_cast<char*>(out.data()), int32_t(out.size()),
				in.data(), int32_t(in.size()),
				&_status);
			icuCheckFailure(_status);
			return out;
		}
		template<Char8 CharT>
		std::u16string toUChars(std::basic_string_view<CharT> in)
		{
			const auto len = ucnv_toUChars(_ptr.getAlias(),
				nullptr, 0,
				reinterpret_cast<const char*>(in.data()), int32_t(in.size()),
				&_status);
			icuCheckFailure(_status);

			std::u16string out(size_t(len), 0);
			ucnv_toUChars(_ptr.getAlias(),
				out.data(), int32_t(out.size()),
				reinterpret_cast<const char*>(in.data()), int32_t(in.size()),
				&_status);
			icuCheckFailure(_status);
			return out;
		}
	private:
		UErrorCode _status;
		icu::LocalUConverterPointer _ptr;
	};

	template<Char8 CharTOut, Char8 CharTIn>
	std::basic_string<CharTOut>toUTF8(std::basic_string_view<CharTIn> in)
	{
		EncodingDetector detector;
		const auto encodings = detector.detectAll(in);

		// try every possible codepage
		std::u16string u16;
		bool success = false;
		for (const auto& encoding : encodings)
		{
			try
			{
				u16 = toUTF16(in, encoding.c_str());
			}
			catch (EncodingError&)
			{
				continue;
			}
			success = true;
			break;
		}
		if (!success)
		{
			throw EncodingError("String encoding coversion failed.");
		}

		return fromUTF16<CharTOut>(u16, "utf8");
	}

	template<Char8 CharT>
	std::basic_string<CharT> fromUTF16(std::u16string in, const char* codepage)
	{
		EncodingConverter converter(codepage);
		return converter.fromUChars<CharT>(in);
	}
	
	template<Char8 CharT>
	std::u16string toUTF16(std::basic_string_view<CharT> in, const char* codepage)
	{
		EncodingConverter converter(codepage);
		return converter.toUChars(in);
	}
}