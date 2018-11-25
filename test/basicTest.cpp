#include "testBase.h"
#include <iostream>

TEST_CASE("Normal case with simple grammer", "[basic]")
{
	string testcode;

	testcode = "000_temp";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3.14159274 3.14159265");

	testcode = "002_varint64";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "5 -3 -5 6 6 8 1263\n"
							"7 0 -10 1");

	testcode = "003_varbyte";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "19 32 7 255 267 36 228");

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
							"1 18446744073709551614 2 2\n"
							"-9223372036854775805 4\n"
							"-9223372036854775808 1\n"
							"36 7 8\n"
							"9223372036854775807");

	testcode = "006_intarray";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3 2 4 5 9 3 4");
	CHECK(mcheck("mtrace006") == "+27 -27");

	testcode = "007_whiletest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "0123456789321");

	testcode = "008_iftest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "[if][elif][elif2][else][true]\n"
							"0!=<<=1==<=>=2!=>>=\n"
							"[uu][us]>tt010");

	testcode = "009_booltest";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "ab10ttdtfDfDtttde\n"
							"100\n"
							"aaa01bbbcctdtdftdtdee\n"
							"110\n"
							"1010\n"
							"100101\n"
							"010110");

	testcode = "010_arrarray";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "11 12 13 3124 2135 1110 2 12 3");
	CHECK(mcheck("mtrace010") == "+86 -86");

	testcode = "011_assignment";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "1 2 1 2 5 5 6 6\n"
							"3 4 2 0\n"
							"5 -1\n"
							"9 9 9 0\n"
							"3 9\n"
							"3 5 10 3 5 10\n"
							"3 9 8 4 2 7 6 1");
	CHECK(mcheck("mtrace011-1") == "+0 -2");
	CHECK(mcheck("mtrace011-2") == "+3 -3");
	CHECK(mcheck("mtrace011-3") == "+0 -0");
	CHECK(mcheck("mtrace011-4") == "+0 -8");

	testcode = "012_overload";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "(int32[10])(uint16)(int32)(uint32,uint32)");

	testcode = "013_chaincall";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3 9 4 10 5 11\n"
								"1 4 8 3 5 8\n"
								"3 2 4 7 10\n"
								"9 6 12\n"
								"1 4 8 1 8 9\n"
								"1 2 3");

	testcode = "014_const";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "test1 11\n"
							"test2 5\n"
							"test3 5\n"
							"test1 3");

	testcode = "015_assignment2";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "2 1 2 1 2\n"
							"4 1 2 1 2 1 2\n"
							"4 1\n9 7 1\n"
							"4 1 4\n"
							"4 1 4 4 1 1 4");

	testcode = "016_varflo";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3.14159274 3.14159265\n"
							"3.14159274 3.14159265\n"
							"3.14159274 3.14159274\n"
							"4.00 -5.00 1.23 2.34 4.00 -5.00 "
							"6.00 7.00 0.00 0.00 "
							"8.00 9.00 1.23 2.34 \n"
							"1 2 3 1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0\n"
							"-10.00 11.00 10.00 11.00 -10.00 11.00 10.00 11.00 -10 11 10 11\n"
							"-12.00 13.00 12.00 13.00 -12.00 13.00 12.00 13.00 -12 13 12 13\n"
							"-14.00 15.00 14.00 15.00 -14.00 15.00 14.00 15.00 -14 15 14 15\n"
							"-16.00 17.00 16.00 17.00 -16.00 17.00 16.00 17.00 -16 17 16 17\n"
							"1 2 3 4 5 6 7 8\n"
							"-1 -2 -3 -4 5 6 7 8\n"
							"-16 17 -16 17 240 17 4294967280 17"
							);

	testcode = "017_floope";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "3.57 3.57 11.23 6.23 11.23 6.23 3 3 7.02 3.57 3.57 7.15 3.23 3.23 2.34 12.46\n"
							"-1.11 1.11 2.89 -0.43 -7.77 -3.77 7.77 3.77 -1 1 -0.12 -1.11 -1.11 -4.69 0.77 -0.77\n"
							"-1.23 1.11 -2.50 -1.23 -0.00 -1.23e+10 2.34e-02 1.23e+00 1.80e+308\n"
							"2.88 2.88 0.84 0.84 2.46 6.15 2.46 6.15 9.93 2.88 3.03 8.42 2.46 2.46\n"
							"0.53 0.53 7.24 0.21 4.07 4.07 0.25 0.25 1.20 0.53 0.50 0.18 1.63 0.61");

	testcode = "018_floarr";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "1.23 2.21 1.23 2.21");
	CHECK(mcheck("mtrace018") == "+1 -1");

	testcode = "019_flocmp";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "tftfft tfft fttf ftft ffttf\n"
								"tftfft tfft fttf ftft fft\n"
								"fftf fttfft fftft tftf tftf");
}

TEST_CASE("Compile error test", "[basic]")
{
	string testcode;

	testcode = "500_syntax_err";
	REQUIRE(build(testcode) == "0:4-4 syntax error, unexpected '=', expecting ->> or -> or ','");

	testcode = "501_dupvar_err";
	REQUIRE(build(testcode) == "0:2-2 Variable name 'b' already defined.");

	testcode = "502_undeffunc_err";
	REQUIRE(build(testcode) == "0:2-2 Function 'add' was not declared in this scope.");

	testcode = "503_undefvar_err";
	REQUIRE(build(testcode) == "0:3-3 Variable 'abcd' was not declared in this scope.");

	testcode = "504_copyfreevar_err";
	REQUIRE(build(testcode) == "finish:0:6-6 Can not copy to freed variable 'arr2'.");

	testcode = "505_ambigfunc_err";
	REQUIRE(build(testcode) == "0:3-3 Ambiguous function call 'ambi_func'.");

	testcode = "506_assigntype_err";
	REQUIRE(build(testcode) == "0:4-4 Incompatible types in assignment of 'int32[10]' to 'int32'.");

	testcode = "507_toplvstmt_err";
	REQUIRE(build(testcode) == "0:3-3 Can not use 'return' at top level code.");

	testcode = "508_needretarg_err";
	REQUIRE(build(testcode) == "0:4-4 Return argument(s) can't be omitted at this function.");

	testcode = "509_needret_err";
	REQUIRE(build(testcode) == "finish:0:6-6 Return argument(s) can't be omitted at this function.");

	testcode = "510_invalidret1_err";
	REQUIRE(build(testcode) == "0:3-3 Number of return arguments or definitions are not match.");

	testcode = "511_invalidret2_err";
	REQUIRE(build(testcode) == "0:3-3 Number of return arguments or definitions are not match.");

	testcode = "512_invalidret3_err";
	REQUIRE(build(testcode) == "0:4-4 Number of return arguments or definitions are not match.");

	testcode = "513_asgnLRnum_err";
	REQUIRE(build(testcode) == "0:2-2 Number of left values did not match right values.");

	testcode = "514_cantusemove_err";
	REQUIRE(build(testcode) == "0:6-6 Can not use '>>' for 'arr2[]'.");

	testcode = "515_undefchainfunc_err";
	REQUIRE(build(testcode) == "0:3-3 Function 'test' was not declared in this scope.");

	testcode = "516_duplicate_const_err";
	REQUIRE(build(testcode) == "0:4-4 Const name 'N' already defined.");

	testcode = "517_invalid_constval_err";
	REQUIRE(build(testcode) == "0:3-3 Can not use dynamic expression for const 'N'.");

	testcode = "518_invalid_constuse_err";
	REQUIRE(build(testcode) == "0:3-3 Can not use the operator for 'N'.");

	testcode = "519_duplicate_func_err";
	REQUIRE(build(testcode) == "0:7-7 Function 'test' already defined.");

	testcode = "520_const_LRnum_err";
	REQUIRE(build(testcode) == "0:1-1 Number of left values did not match right values.");

	testcode = "521_asgn_novalue_err";
	REQUIRE(build(testcode) == "0:3-3 Number of left values did not match right values.");

	testcode = "522_cantuse_index_err";
	REQUIRE(build(testcode) == "0:2-2 Can not use the index operator for 'a'.");

	testcode = "523_varinitLRnum_err";
	REQUIRE(build(testcode) == "0:1-1 Number of left values did not match right values.");

	testcode = "524_flo_mod_err";
	REQUIRE(build(testcode) == "0:3-3 Can not use the operator for 'float number'.");
}
