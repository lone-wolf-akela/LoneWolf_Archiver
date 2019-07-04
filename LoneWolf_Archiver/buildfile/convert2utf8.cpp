#include <cassert>

#include <sstream>
#include <locale>
#include <tuple>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include <boost/locale.hpp>

#include "buildfile.h"

namespace
{
#if defined(_WIN32)
	///\brief	Convert ANSI string to wstring
	std::wstring ConvertANSIToWchar(std::string_view in)
	{
		int buffersize;
		std::wstring r;

		buffersize = MultiByteToWideChar(
			CP_ACP,			//CodePage,
			0,				//dwFlags,
			in.data(),		//lpMultiByteStr,
			int(in.size()),	//cbMultiByte,
			nullptr,		//lpWideCharStr,
			0);				//cchWideChar
		if (!buffersize)
		{
			goto ConvertANSIToWcharErr;
		}
		r.resize(buffersize);
		if (!MultiByteToWideChar(
			CP_ACP,			//CodePage,
			0,				//dwFlags,
			in.data(),		//lpMultiByteStr,
			int(in.size()),	//cbMultiByte,
			r.data(),		//lpWideCharStr,
			int(r.size())))	//cchWideChar
		{
			goto ConvertANSIToWcharErr;
		}
		return r;
	ConvertANSIToWcharErr:
		const auto errcode = GetLastError();
		///\note	the error can be:
		///			ERROR_INSUFFICIENT_BUFFER
		///			ERROR_INVALID_FLAGS
		///			ERROR_INVALID_PARAMETER
		///			ERROR_NO_UNICODE_TRANSLATION
		throw SystemApiError(
			std::string("Error happened when calling `MultiByteToWideChar': ") + (
			(errcode == ERROR_INSUFFICIENT_BUFFER) ? "ERROR_INSUFFICIENT_BUFFER" :
				(errcode == ERROR_INVALID_FLAGS) ? "ERROR_INVALID_FLAGS" :
				(errcode == ERROR_INVALID_PARAMETER) ? "ERROR_INVALID_PARAMETER" :
				(errcode == ERROR_NO_UNICODE_TRANSLATION) ? "ERROR_NO_UNICODE_TRANSLATION" :
				"Unkown Error"));
	}
#endif
}

namespace buildfile
{
	///\brief	Convert string to utf8.
	///		(But still store it in a std::string instead of std::u8string, for convenience).
	///\note	UTF16 is only supported on Windows. 
	std::string ConvertToUTF8(std::string_view in)
	{
#if defined(_WIN32)
		int test = IS_TEXT_UNICODE_REVERSE_MASK;
		IsTextUnicode(in.data(), int(in.size()), &test);
		if (test)
		{
			// is reverse UNICODE
			std::wstringstream stream;
			for (size_t i = 0; i < in.size(); i += 2)
			{
				wchar_t t = *reinterpret_cast<const wchar_t*>(in.data() + i);
				stream << wchar_t((t << 8) | ((t >> 8) & 0x00ff));
			}
			return boost::locale::conv::utf_to_utf<char>(stream.str());
		}
		test = IS_TEXT_UNICODE_UNICODE_MASK;
		IsTextUnicode(in.data(), int(in.size()), &test);
		if (test)
		{
			// is UNICODE
			auto wptr = reinterpret_cast<const WCHAR*>(in.data());
			auto wlen = in.size() * sizeof(in[0]) / sizeof(WCHAR);
			return boost::locale::conv::utf_to_utf<char>(wptr, wptr + wlen);
		}
#endif
		// not UNICODE, need codepage detection
		// we will try to parse the file as a UTF8 file
		try
		{
			std::ignore = boost::locale::conv::to_utf<char>(
				in.data(),
				in.data() + in.size(),
				"UTF-8",
				boost::locale::conv::method_type::stop);
			// Success. This is a UTF8 string. no need for conversion.
			return std::string(in);
		}
		catch (boost::locale::conv::conversion_error)
		{
			// seems not UTF8, will try to parse as system default ANSI code page	
#if defined(_WIN32)
			// It seems that boost cannot reliably detect current codepage on windows,
			// so I have to use Windows API.
			return boost::locale::conv::utf_to_utf<char>(ConvertANSIToWchar(in));
#else
			return boost::locale::conv::to_utf<char>(
				in.data(),
				in.data() + in.size(),
				std::locale(""));
#endif
		}
	}
}
