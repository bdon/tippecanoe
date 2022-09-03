#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "text.hpp"
#include "pmtiles.hpp"

TEST_CASE("UTF-8 enforcement", "[utf8]") {
	REQUIRE(check_utf8("") == std::string(""));
	REQUIRE(check_utf8("hello world") == std::string(""));
	REQUIRE(check_utf8("Καλημέρα κόσμε") == std::string(""));
	REQUIRE(check_utf8("こんにちは 世界") == std::string(""));
	REQUIRE(check_utf8("👋🌏") == std::string(""));
	REQUIRE(check_utf8("Hola m\xF3n") == std::string("\"Hola m\xF3n\" is not valid UTF-8 (0xF3 0x6E)"));
}

TEST_CASE("UTF-8 truncation", "[trunc]") {
	REQUIRE(truncate16("0123456789abcdefghi", 16) == std::string("0123456789abcdef"));
	REQUIRE(truncate16("0123456789éîôüéîôüç", 16) == std::string("0123456789éîôüéî"));
	REQUIRE(truncate16("0123456789😀😬😁😂😃😄😅😆", 16) == std::string("0123456789😀😬😁"));
	REQUIRE(truncate16("0123456789😀😬😁😂😃😄😅😆", 17) == std::string("0123456789😀😬😁"));
	REQUIRE(truncate16("0123456789あいうえおかきくけこさ", 16) == std::string("0123456789あいうえおか"));
}

TEST_CASE("Z,X,Y to TileID", "") {
	auto result = tileid_to_zxy(0);
	REQUIRE(result.z == 0);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(1);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(2);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 1);
	result = tileid_to_zxy(3);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 1);
	REQUIRE(result.y == 1);
	result = tileid_to_zxy(4);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 1);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(5);
	REQUIRE(result.z == 2);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
}