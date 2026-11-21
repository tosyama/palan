/// std/rt tests
///
/// @file rtTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(std_rt, top_level_tests) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../src/std/rt.pa"), "");
}
