#include "cuiTestBase.h"

TEST_CASE("CUI basic err test.", "[cui]")
{
	string testcode = "XXX_src_not_found";
	REQUIRE(build(testcode) == "compile err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "pat: error: Could not open file 'pacode/XXX_src_not_found.pa'.\n");

	// err from pat
	testcode = "500_syntax_err";
	REQUIRE(build(testcode) == "compile err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "500_syntax_err.pa:4: syntax error, unexpected '=', expecting ->> or -> or ','\n");

	// err from build tree
	testcode = "501_dupvar_err";
	REQUIRE(build(testcode) == "compile err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "501_dupvar_err.pa:2: Variable name 'b' already defined.");

	// err from finish phase.
	testcode = "509_needret_err";
	REQUIRE(build(testcode) == "compile err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "509_needret_err.pa:6: Return argument(s) can't be omitted at this function.");
}
