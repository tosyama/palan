#include "testBase.h"

TEST_CASE("Normal case with simple grammer", "[basic]")
{
	string testcode = "002_varint64";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "5 -3 -5 6 8");
}

