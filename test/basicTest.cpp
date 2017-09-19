#include "testBase.h"

TEST_CASE("Normal case with simple grammer", "[basic]")
{
	string testcode;

	testcode = "002_varint64";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "5 -3 -5 6 6 8");

	testcode = "003_varbyte";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "19 32 7 255 267");

	testcode = "004_regalloc";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "56 11 2 3 4 5 6 7 18");
}

