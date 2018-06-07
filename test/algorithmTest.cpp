#include "testBase.h"

TEST_CASE("basic algorithm code example.", "[algorithm][.]")
{
	string testcode;

	testcode = "100_qsort";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "before: 0 4 8 3 7 2 6 1 5 0\n"
							"after: 0 0 1 2 3 4 5 6 7 8");
	testcode = "101_8queen";
 	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "answer: 92");
}
