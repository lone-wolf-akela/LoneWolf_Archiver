#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>
#include <filesystem>
#include <vector>
#include <cctype>
#include <string>
#include <iomanip>

#include <openssl/md5.h>

template<typename Lambda>
constexpr auto GetStdout(Lambda code)
{
	const auto cout_buf = std::cout.rdbuf();
	const auto cerr_buf = std::cerr.rdbuf();
	const std::ostringstream strm;
	std::cout.rdbuf(strm.rdbuf());
	std::cerr.rdbuf(strm.rdbuf());
	code();
	std::cout.rdbuf(cout_buf);
	std::cerr.rdbuf(cerr_buf);
	return strm.str();
};

template<typename Iter>
std::string make_hex_string(
	const Iter begin,
	const Iter end,
	const bool use_uppercase = false,
	const bool insert_spaces = false
)
{
	std::ostringstream ss;
	ss << std::hex << std::setfill('0');
	if (use_uppercase) { ss << std::uppercase; }
	for (auto i = begin; i != end; ++i)
	{
		ss << std::setw(2) << static_cast<int>(*i);
		if (insert_spaces && i != end) { ss << " "; }
	}
	return ss.str();
}

// Try to find in the Haystack the Needle - ignore case
inline bool findStringCaseInsensitive(std::string_view strHaystack, std::string_view strNeedle)
{
	const auto it = std::search(
		strHaystack.begin(), strHaystack.end(),
		strNeedle.begin(), strNeedle.end(),
		[](auto ch1, auto ch2) { return std::toupper(ch1) == std::toupper(ch2); }
	);
	return (it != strHaystack.end());
}

inline bool checkMD5(const std::filesystem::path& path, std::string_view md5_expect)
{
	const auto filesize = file_size(path);
	std::vector<std::byte> filedata(filesize);

	{
		std::ifstream ifs;
		ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		ifs.open(path, std::ios::binary);
		ifs.read(reinterpret_cast<char*>(filedata.data()), filesize);
	}

	MD5_CTX md5_context;
	MD5_Init(&md5_context);
	MD5_Update(&md5_context, filedata.data(), filesize);
	std::array<unsigned char, MD5_DIGEST_LENGTH> md5_out{};
	MD5_Final(md5_out.data(), &md5_context);
	const std::string md5_out_str = make_hex_string(md5_out.begin(), md5_out.end());
	return std::equal(
		md5_out_str.begin(), md5_out_str.end(),
		md5_expect.begin(), md5_expect.end(),
		[](auto a, auto b) {
			return std::toupper(a) == std::toupper(b);
		});
}
