#include "testBase.h"

TEST_CASE("Normal case with simple grammer", "[basic]")
{
	string testcode;

	testcode = "000_temp";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "");

	testcode = "002_varint64";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "5 -3 -5 6 6 8 1263\n"
							"7 0 -10");

	testcode = "003_varbyte";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "19 32 7 255 267 36");

	testcode = "004_regalloc";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "56 11 2 3 4 5 6 7 18\n"
							"64 32");

	testcode = "005_intpromotion";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "-128 255 -32768 65535\n"
							"-2147483648 4294967295\n"
							"-9223372036854775808\n"
							"18446744073709551615\n"
							"6148914691236517205 0\n"
							"0 -1\n"
							"0 -1");

	testcode = "006_intarray";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3 2 4 5 9 3 4");
	CHECK(mcheck("mtrace006") == "+19 -19");

	testcode = "007_whiletest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "0123456789");

	testcode = "008_iftest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "[if][elif][elif2][else][true]\n"
							"0!=<<=1==<=>=2!=>>=\n"
							"[uu][us]>tt010");

	testcode = "009_booltest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "abttdtfDfDtttde\n"
							"100\n"
							"aaabbbcctdtdftdtdee\n"
							"110\n"
							"101010");

	testcode = "010_arrarray";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "1 2 1 1");
	CHECK(mcheck("mtrace010") == "+99 -100");

	testcode = "011_assignment";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "1 2 1 2\n"
							"3 5 2 1\n"
							"5 -1\n"
							"9 9 9 0");
}

