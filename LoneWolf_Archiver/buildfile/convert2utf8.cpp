#include <locale>

#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>

#include "buildfile.h"


using namespace std::literals;

namespace
{
	void IcuCheckFailure(UErrorCode status)
	{
		if (U_FAILURE(status))
		{
			throw SystemApiError("Error opening: "s + u_errorName(status));
		}
	}
}

namespace buildfile
{
	///\brief	Convert string to utf8.
	///		(But still store it in a std::string instead of std::u8string, for convenience).
	std::string ConvertToUTF8(std::string_view in)
	{
		UErrorCode status = U_ZERO_ERROR;
		const icu_61::LocalUCharsetDetectorPointer detector(ucsdet_open(&status));
		IcuCheckFailure(status);

		ucsdet_setText(detector.getAlias(), in.data(), int32_t(in.size()), &status);
		IcuCheckFailure(status);

		int32_t matches_num;
		const auto matches = ucsdet_detectAll(detector.getAlias(), &matches_num, &status);
		IcuCheckFailure(status);

		// try every possible codepage
		std::u16string u16;
		bool success = false;
		for (int32_t i = 0; i < matches_num; i++)
		{
			const auto name = ucsdet_getName(matches[i], &status);
			IcuCheckFailure(status);
			const icu_61::LocalUConverterPointer conv(ucnv_open(name, &status));
			IcuCheckFailure(status);
			try
			{
				const auto len = ucnv_toUChars(conv.getAlias(),
					nullptr, 0,
					in.data(), int32_t(in.size()),
					&status);
				IcuCheckFailure(status);

				u16.resize(size_t(len) + 1);
				ucnv_toUChars(conv.getAlias(),
					u16.data(), int32_t(u16.size()),
					in.data(), int32_t(in.size()),
					&status);
				IcuCheckFailure(status);
			}
			catch (SystemApiError&)
			{
				continue;
			}
			success = true;
			break;
		}
		if (!success)
		{
			throw SystemApiError("File encode coversion failed.");
		}

		std::string u8;

		const icu_61::LocalUConverterPointer conv(ucnv_open("utf8", &status));
		IcuCheckFailure(status);

		const auto len = ucnv_fromUChars(conv.getAlias(),
			nullptr, 0,
			u16.data(), int32_t(u16.size()),
			&status);
		IcuCheckFailure(status);

		u8.resize(size_t(len) + 1);
		ucnv_fromUChars(conv.getAlias(),
			u8.data(), int32_t(u8.size()),
			u16.data(), int32_t(u16.size()),
			&status);
		IcuCheckFailure(status);

		return u8;
	}
}
