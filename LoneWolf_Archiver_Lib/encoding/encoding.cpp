#include "encoding.h"

namespace encoding
{
	std::u16string wide_to_utf16(std::wstring_view in)
	{
		return boost::locale::conv::utf_to_utf<char16_t>(
			in.data(), in.data() + in.length()
			);
	}

	std::wstring utf16_to_wide(std::u16string_view in)
	{
		return boost::locale::conv::utf_to_utf<wchar_t>(
			in.data(), in.data() + in.length()
			);
	}
}
