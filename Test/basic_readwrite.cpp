#define BOOST_TEST_MODULE basic read write
#include <boost/test/included/unit_test.hpp>

#include <optional>
#include <map>

#include "../LoneWolf_Archiver_Lib/LoneWolfArchiver.h"

#include "helper.h"

void GenBuildFile(const char* root)
{
	std::array args = {
		"",
		"-s", "temp/buildfile.txt",
		"-r", root
	};

	BOOST_REQUIRE_NO_THROW(
		lonewolf_archiver(int(args.size()), args.data());
	);
}

void Extract(const char* big)
{
	std::array args = {
		"",
		"-e", "temp",
		"-a", big
	};

	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&] {lonewolf_archiver(int(args.size()), args.data()); });
	);

	const bool extract_success = findStringCaseInsensitive(*out, "Operation took");
	BOOST_TEST(extract_success);
}

void checkFilesMD5(std::string root)
{
	const std::map<std::filesystem::path, std::string> md5list = {
		{root + "/ati.dat", "3b4f27e7d682bd13a35e5976e4ceab75"},
		{root + "/buildresearch.dat", "e4688b9c119738936828047c3f974c6d"},
		{root + "/conversion record.xlsx", "918a85dbcfb109f6d452b855bb8c9b0e"},
		{root + "/engine.dat", "dc7129038658aba93f021e0581681524"},
		{root + "/events.dat", "5291d517fbb238de020bcd4181e88007"},
		{root + "/fontmap.lua", "311f282bc8622e073a3f99e2bea4bc34"},
		{root + "/hw1buildresearch.dat", "d87cc87ee0b395346e9252ce6e089bc3"},
		{root + "/hw1ships.dat", "8dca9eb43660e7a7d9eb3aa0f0aede54"},
		{root + "/leveldata/campaign/rr_oem/mission05.dat", "5545b4e4a1ad7ddb5e7ef949a1cdd979"},
		{root + "/leveldata/campaign/rr_oem/mission05oem.dat", "561494b5e02138034203f3539dfd38b1"},
		{root + "/leveldata/campaign/rr_oem/mission05oemlstrings.dat", "ce29174717406b015a70e78a91a55729"},
		{root + "/leveldata/campaign/tutorial/m01.dat", "fe6137ccc9f9bfe1d3ad4610cb5b83a4"},
		{root + "/leveldata/campaign/tutorial/m02.dat", "6f917b5de116a90e0a8198b6d30927d8"},
		{root + "/leveldata/campaign/tutorial/m03.dat", "b31bbb614690bc379b50f181d489b613"},
		{root + "/leveldata/multiplayer/lib/bounties.dat", "5735c1a92fb8648dd62171de5d832e2f"},
		{root + "/leveldata/multiplayer/lib/challenges.dat", "3eecc784ab5c6e3b391ca7e263b237cf"},
		{root + "/leveldata/multiplayer/lib/crates.dat", "3ccced7641ee3c3e2fd89f51b0a116d9"},
		{root + "/leveldata/multiplayer/lib/norushtime.dat", "746183e4d704d8168ee5a5fdc830467d"},
		{root + "/leveldata/multiplayer/lib/relics.dat", "4752c6a2c0026296b7c9157b552cda46"},
		{root + "/leveldata/multiplayer/lib/research.dat", "9a98c078f9ca4bc914eec10f810985d5"},
		{root + "/leveldata/multiplayer/lib/ruinjections.dat", "9fdb9106487bedcff4bd39b7b12f36b4"},
		{root + "/leveldesc.dat", "8bd651cf44fc02650263293050b413f1"},
		{root + "/resource.dat", "ead63501c753ecd0969c8da9de8756bb"},
		{root + "/scripts/keybindings.lua", "bd2f03d4181bbb6bdb21f6403334aa9d"},
		{root + "/ships.dat", "06c55ed32711667e051828a087057cbf"},
		{root + "/ui.dat", "cbc35f5bcc79261ecde44e19d9013e4e"}
	};
	for (const auto& [path, md5] : md5list)
	{
		BOOST_TEST(checkMD5(path, md5));
	}
}

BOOST_AUTO_TEST_CASE(parse_config)
{
	std::array args = { "" };
	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&]() {lonewolf_archiver(int(args.size()), args.data()); });
	);
	const bool parse_config_success = !findStringCaseInsensitive(*out, "Failed to read");

	BOOST_TEST(parse_config_success);
}

BOOST_AUTO_TEST_CASE(simple_extract)
{
	std::filesystem::remove_all("temp");

	std::array args = {
		"",
		"-e", "temp",
		"-a", "data/english.big"
	};

	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&] {lonewolf_archiver(int(args.size()), args.data()); });
	);

	const bool extract_success = findStringCaseInsensitive(*out, "Operation took");
	BOOST_TEST(extract_success);

	checkFilesMD5("temp/hw2locale");

	std::filesystem::remove_all("temp");
}

BOOST_AUTO_TEST_CASE(gen_buildfile)
{
	std::filesystem::remove_all("temp");
	BOOST_REQUIRE_NO_THROW(GenBuildFile("data/english"));
	std::filesystem::remove_all("temp");
}

BOOST_AUTO_TEST_CASE(build_use_buildfile)
{
	std::filesystem::remove_all("temp");
	BOOST_REQUIRE_NO_THROW(GenBuildFile("data/english"));
	std::array args = {
		"",
		"-c", "temp/buildfile.txt",
		"-r", "data/english",
		"-a", "temp/built.big"
	};

	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&] {lonewolf_archiver(int(args.size()), args.data()); });
	);

	const bool build_success = findStringCaseInsensitive(*out, "Operation took");
	BOOST_TEST(build_success);

	Extract("temp/built.big");
	checkFilesMD5("temp/tocenglish");
	std::filesystem::remove_all("temp");
}

void GenBig(std::string big, std::string root)
{
	std::array args = {
		"",
		"-g",
		"-r", root.c_str(),
		"-a", big.c_str()
	};

	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&] {lonewolf_archiver(int(args.size()), args.data()); });
	);

	const bool build_success = findStringCaseInsensitive(*out, "Operation took");
	BOOST_TEST(build_success);
}

BOOST_AUTO_TEST_CASE(generate_big)
{
	std::filesystem::remove_all("temp");
	GenBig("temp/built.big", "data/english");
	
	Extract("temp/built.big");
	checkFilesMD5("temp/tocenglish");
	std::filesystem::remove_all("temp");
}

#if defined(_WIN32)
BOOST_AUTO_TEST_CASE(build_use_ourbuildfile_and_relicarhive)
{
	std::filesystem::remove_all("temp");
	BOOST_REQUIRE_NO_THROW(GenBuildFile("data/english"));

	const std::string cmd = "data\\Archive.exe -c temp/buildfile.txt -r " +
		std::filesystem::absolute("data/english").string() +
		" -a temp/built.big";
	system(cmd.c_str());

	Extract("temp/built.big");
	checkFilesMD5("temp/tocenglish");
	std::filesystem::remove_all("temp");
}

BOOST_AUTO_TEST_CASE(extract_use_relicarhive)
{
	std::filesystem::remove_all("temp");
	GenBig("temp/built.big", "data/english");

	const std::string cmd = "data\\Archive.exe -e temp/tocenglish -a temp/built.big";
	system(cmd.c_str());
	checkFilesMD5("temp/tocenglish");
	std::filesystem::remove_all("temp");
}
#endif
