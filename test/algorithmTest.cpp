#include "testBase.h"

static void algorithmTest()
{
	string testcode;

	testcode = "100_qsort";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "before: 0 4 8 3 7 2 6 1 5 0\n"
							"after: 0 0 1 2 3 4 5 6 7 8");
	testcode = "101_8queen";
 	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "answer: 92");

	testcode = "102_lsm";
 	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "-0.633, 1.204");
}

TEST_CASE("basic algorithm code example.", "[algorithm][.]")
{
	disableOptimize = true;
	algorithmTest();
	disableOptimize = false;
	algorithmTest();
}
