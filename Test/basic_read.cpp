#define BOOST_TEST_MODULE basic read
#include <boost/test/included/unit_test.hpp>

#include <optional>
#include <map>

#include "../LoneWolf_Archiver_Lib/LoneWolfArchiver.h"

#include "helper.h"

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
		"-a", "english.big"
	};

	std::optional<std::string> out;
	BOOST_REQUIRE_NO_THROW(
		out = GetStdout([&] {lonewolf_archiver(int(args.size()), args.data()); });
	);

	const bool extract_success = findStringCaseInsensitive(*out, "Operation took");
	BOOST_TEST(extract_success);

	const std::map<std::filesystem::path, std::string> md5list = {
		{"temp/hw2locale/ati.dat", "3b4f27e7d682bd13a35e5976e4ceab75"},
		{"temp/hw2locale/buildresearch.dat", "e4688b9c119738936828047c3f974c6d"},
		{"temp/hw2locale/conversion record.xlsx", "918a85dbcfb109f6d452b855bb8c9b0e"},
		{"temp/hw2locale/engine.dat", "dc7129038658aba93f021e0581681524"},
		{"temp/hw2locale/events.dat", "5291d517fbb238de020bcd4181e88007"},
		{"temp/hw2locale/fontmap.lua", "311f282bc8622e073a3f99e2bea4bc34"},
		{"temp/hw2locale/hw1buildresearch.dat", "d87cc87ee0b395346e9252ce6e089bc3"},
		{"temp/hw2locale/hw1ships.dat", "8dca9eb43660e7a7d9eb3aa0f0aede54"},
		{"temp/hw2locale/leveldata/campaign/rr_oem/mission05.dat", "5545b4e4a1ad7ddb5e7ef949a1cdd979"},
		{"temp/hw2locale/leveldata/campaign/rr_oem/mission05oem.dat", "561494b5e02138034203f3539dfd38b1"},
		{"temp/hw2locale/leveldata/campaign/rr_oem/mission05oemlstrings.dat", "ce29174717406b015a70e78a91a55729"},
		{"temp/hw2locale/leveldata/campaign/tutorial/m01.dat", "fe6137ccc9f9bfe1d3ad4610cb5b83a4"},
		{"temp/hw2locale/leveldata/campaign/tutorial/m02.dat", "6f917b5de116a90e0a8198b6d30927d8"},
		{"temp/hw2locale/leveldata/campaign/tutorial/m03.dat", "b31bbb614690bc379b50f181d489b613"},
		{"temp/hw2locale/leveldata/multiplayer/lib/bounties.dat", "5735c1a92fb8648dd62171de5d832e2f"},
		{"temp/hw2locale/leveldata/multiplayer/lib/challenges.dat", "3eecc784ab5c6e3b391ca7e263b237cf"},
		{"temp/hw2locale/leveldata/multiplayer/lib/crates.dat", "3ccced7641ee3c3e2fd89f51b0a116d9"},
		{"temp/hw2locale/leveldata/multiplayer/lib/norushtime.dat", "746183e4d704d8168ee5a5fdc830467d"},
		{"temp/hw2locale/leveldata/multiplayer/lib/relics.dat", "4752c6a2c0026296b7c9157b552cda46"},
		{"temp/hw2locale/leveldata/multiplayer/lib/research.dat", "9a98c078f9ca4bc914eec10f810985d5"},
		{"temp/hw2locale/leveldata/multiplayer/lib/ruinjections.dat", "9fdb9106487bedcff4bd39b7b12f36b4"},
		{"temp/hw2locale/leveldesc.dat", "8bd651cf44fc02650263293050b413f1"},
		{"temp/hw2locale/resource.dat", "ead63501c753ecd0969c8da9de8756bb"},
		{"temp/hw2locale/scripts/keybindings.lua", "bd2f03d4181bbb6bdb21f6403334aa9d"},
		{"temp/hw2locale/ships.dat", "06c55ed32711667e051828a087057cbf"},
		{"temp/hw2locale/ui.dat", "cbc35f5bcc79261ecde44e19d9013e4e"}
	};
	for (const auto& [path, md5] : md5list)
	{
		BOOST_TEST(checkMD5(path, md5));
	}

	std::filesystem::remove_all("temp");
}
