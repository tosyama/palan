#include <boost/algorithm/string.hpp>
#include "catch.hpp"
#include "cuiTestBase.h"

using namespace boost;

TEST_CASE("CUI basic command-line test.", "[cui]")
{
	string str;
	vector<string> strs; 

	// pac -h
	REQUIRE(exec_pac("", "-h", "", "") == "success");
	str = outstr("log");
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() == 20);
	REQUIRE(strs[0] == "Usage:");
	REQUIRE(strs[8] == "Options:");
	REQUIRE(strs[9] == "  -h [ --help ]         Display this help");
	REQUIRE(errstr("log") == "");

	// pac -v
	REQUIRE(exec_pac("", "-v", "", "") == "success");
	REQUIRE(outstr("log") == "Palan compiler 0.4.0a\n");
	REQUIRE(errstr("log") == "");

	// pac -S <input-file>"
	string testcode = "001_helloworld";
	REQUIRE(exec_pac(testcode, "-S", "", "") == "success");
	str = outstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == ".section .rodata");
	REQUIRE(errstr(testcode) == "");
	REQUIRE(outfile(testcode + ".o") == "not exists");
	REQUIRE(outfile(testcode) == "not exists");

	// pac -c <input-file>"
	testcode = "002_varint64";
	REQUIRE(exec_pac(testcode, "-c", "", "") == "success");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "");
	REQUIRE(outfile(testcode + ".o") == "exists");
	REQUIRE(outfile(testcode) == "not exists");

	// pac <input-file> -o <output-file>"
	testcode = "003_varbyte";
	REQUIRE(exec_pac(testcode, "-o", testcode, "") == "success");
	REQUIRE(outstr(testcode) == "linking: out/cui/003_varbyte\n");
	REQUIRE(errstr(testcode) == "");
	REQUIRE(outfile(testcode + ".o") == "exists");
	REQUIRE(outfile(testcode) == "exists");

	// math library loading
	testcode = "027_ccall";
	REQUIRE(exec_pac(testcode, "", "", "") == "success");
	REQUIRE(outstr(testcode) == "abc123def1.23\n"
	  							"bbaa 99 2.34 7\n"
	    						"2:This,is\n"
								"infunc\n"
		  						"smy0.33 1.00 abc 1234");

	// object file loading
	testcode = "032_ptrptr";
	REQUIRE(exec_pac(testcode, "", "", "") == "success");
	REQUIRE(outstr(testcode) == "test 1234 test2 123456");

	// pac <input-file>
	testcode = "100_qsort";
	REQUIRE(exec_pac(testcode, "", "", "") == "success");
	REQUIRE(outstr(testcode) == "before: 0 4 8 3 7 2 6 1 5 0\n"
								"after: 0 0 1 2 3 4 5 6 7 8\n");
	REQUIRE(errstr(testcode) == "");
	REQUIRE(outfile(testcode + ".o") == "not exists");
	REQUIRE(outfile(testcode) == "not exists");
	REQUIRE(outfile("a.out") == "not exists");

	// pac <input-file> -o <output-file> -x
	testcode = "101_8queen";
	REQUIRE(exec_pac(testcode, "-o", testcode, "-x") == "success");
	REQUIRE(outstr(testcode) == "answer: 92\n");
	REQUIRE(errstr(testcode) == "");
	REQUIRE(outfile(testcode + ".o") == "exists");
	REQUIRE(outfile(testcode) == "exists");
}

TEST_CASE("CUI parametor validation test.", "[cui]")
{
	string str;
	vector<string> strs; 

	string testcode = "XXX_param_err";

	// pac -z
	REQUIRE(exec_pac(testcode, "-o", testcode, "-z") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	str = errstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "Unrecognised option '-z'");
	REQUIRE(strs[1] == "Usage:");

	// pac
	REQUIRE(exec_pac("", "", "", "") == "err: 255");
	REQUIRE(outstr("log") == "");
	str = errstr("log");
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "No input file");
	REQUIRE(strs[1] == "Usage:");

	// pac input -o
	REQUIRE(exec_pac(testcode, "-o", "", "") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	str = errstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "The required argument for option '--output' is missing");
	REQUIRE(strs[1] == "Usage:");

	// pac input -c -o output
	REQUIRE(exec_pac(testcode, "-c -o", testcode, "") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	str = errstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "Incompatible options are specifiled");
	REQUIRE(strs[1] == "Usage:");

	// pac input -S -c
	REQUIRE(exec_pac(testcode, "-S -c", "", "") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	str = errstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "Incompatible options are specifiled");
	REQUIRE(strs[1] == "Usage:");

	// pac input -S -o output
	REQUIRE(exec_pac(testcode, "-S -o", testcode, "") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	str = errstr(testcode);
	split(strs, str, is_any_of("\n"));
	REQUIRE(strs.size() > 20);
	REQUIRE(strs[0] == "Incompatible options are specifiled");
	REQUIRE(strs[1] == "Usage:");

	// pac input -S -x
	REQUIRE(exec_pac(testcode, "-S -x", "", "") == "err: 255");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "Excecute option use only with output option\n");

	REQUIRE(outfile(testcode + ".o") == "not exists");
	REQUIRE(outfile(testcode) == "not exists");
}

TEST_CASE("CUI basic err test.", "[cui]")
{
	string testcode = "XXX_src_not_found";
	REQUIRE(build(testcode) == "err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "pat: error: Could not open file 'out/cui/XXX_src_not_found.pa'.\n");

	// err from pat
	testcode = "500_syntax_err";
	REQUIRE(build(testcode) == "err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "500_syntax_err.pa:4: syntax error, unexpected function identifier, expecting ';'\n");

	// err from build tree
	testcode = "501_dupvar_err";
	REQUIRE(build(testcode) == "err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "501_dupvar_err.pa:2: Variable name 'b' already defined.\n");

	// err from finish phase.
	testcode = "509_needret_err";
	REQUIRE(build(testcode) == "err: 1");
	REQUIRE(outstr(testcode) == "");
	REQUIRE(errstr(testcode) == "509_needret_err.pa:6: Return argument(s) can't be omitted at this function.\n");
}

TEST_CASE("sample code compile test.", "[cui]")
{
	string dir = "../../samples/";
	string testcode = "tetris";
	REQUIRE(exec_pac(testcode, "-c", "", "", dir) == "success");
	REQUIRE(outfile(testcode + ".o") == "exists");

	testcode = "usesqlite";
	REQUIRE(exec_pac(testcode, "-c", "", "", dir) == "success");
	REQUIRE(outfile(testcode + ".o") == "exists");

	testcode = "raytracer";
	REQUIRE(exec_pac(testcode, "-c", "", "", dir) == "success");
	REQUIRE(outfile(testcode + ".o") == "exists");
}
