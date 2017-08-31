#include "testBase.h"
#include "../generators/PlnX86_64DataAllocator.h"

TEST_CASE("Register allocation basic test.(Normal call)", "[allocate]")
{
	PlnDataPlace
		dp1(DPS_SIGNED, 8, 8), dp2(DPS_SIGNED, 8, 8),
		dp3(DPS_SIGNED, 8, 8), dp4(DPS_SIGNED, 8, 8),
		dp5(DPS_SIGNED, 8, 8), dp6(DPS_SIGNED, 8, 8),
		dp7(DPS_SIGNED, 8, 8), dp8(DPS_SIGNED, 8, 8),
		dp9(DPS_SIGNED, 8, 8), dp10(DPS_SIGNED, 8, 8);

	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& allocator = x64allocator;
	
	allocator.reset();

	allocator.pushArgDp(1,&dp1);
	allocator.pushArgDp(1,&dp2);
	allocator.pushArgDp(1,&dp3);
	allocator.pushArgDp(3,&dp4);
	allocator.pushArgDp(2,&dp5);
	allocator.pushArgDp(6,&dp6);
	allocator.pushArgDp(7,&dp7);
	allocator.pushArgDp(7,&dp8);
	allocator.pushArgDp(7,&dp9);
	allocator.pushArgDp(6,&dp10);

	PlnDataPlace* dp;
	dp = allocator.popArgDp(6);
	REQUIRE(dp == &dp10);
	REQUIRE(dp->index == R9);
	REQUIRE(dp->place_index == R9);

	dp = allocator.popArgDp(7);
	REQUIRE(dp == &dp9);
	REQUIRE(dp->index == 0);
	REQUIRE(dp->place_index == 0);
	
	dp = allocator.popArgDp(7);
	REQUIRE(dp == &dp8);
	REQUIRE(dp->type == DP_TEMP_STK);
	REQUIRE(dp->index == 1);
	REQUIRE(dp->place_index == 0);
	
	dp = allocator.popArgDp(7);
	REQUIRE(dp == &dp7);
	REQUIRE(dp->index == R8);
	REQUIRE(dp->place_index == 0);

	dp = allocator.popArgDp(6);
	REQUIRE(dp == &dp6);
	REQUIRE(dp->type == DP_TEMP_STK);
	REQUIRE(dp->index == 2);
	REQUIRE(dp->place_index == R9);

	dp = allocator.popArgDp(2);
	REQUIRE(dp == &dp5);
	REQUIRE(dp->index == RSI);
	REQUIRE(dp->place_index == RSI);

	dp = allocator.popArgDp(3);
	REQUIRE(dp == &dp4);
	REQUIRE(dp->index == RDX);
	REQUIRE(dp->place_index == RDX);

	dp = allocator.popArgDp(1);
	REQUIRE(dp == &dp3);
	REQUIRE(dp->index == RDI);
	REQUIRE(dp->place_index == RDI);

	dp = allocator.popArgDp(1);
	REQUIRE(dp == &dp2);
	REQUIRE(dp->index == RCX);
	REQUIRE(dp->place_index == RDI);
	
	dp = allocator.popArgDp(1);
	REQUIRE(dp == &dp1);
	REQUIRE(dp->index == R11);
	REQUIRE(dp->place_index == RDI);
}

TEST_CASE("Register allocation basic test.(System call)", "[allocate]")
{
	PlnDataPlace
		dp1(DPS_SIGNED, 8, 8), dp2(DPS_SIGNED, 8, 8),
		dp3(DPS_SIGNED, 8, 8), dp4(DPS_SIGNED, 8, 8),
		dp5(DPS_SIGNED, 8, 8), dp6(DPS_SIGNED, 8, 8),
		dp7(DPS_SIGNED, 8, 8), dp8(DPS_SIGNED, 8, 8),
		dp9(DPS_SIGNED, 8, 8), dp10(DPS_SIGNED, 8, 8);

	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& allocator = x64allocator;
	
	allocator.reset();

	allocator.pushSysArgDp(1,&dp1);
	allocator.pushSysArgDp(2,&dp2);
	allocator.pushSysArgDp(3,&dp3);
	allocator.pushSysArgDp(4,&dp4);
	allocator.pushSysArgDp(5,&dp5);
	allocator.pushSysArgDp(6,&dp6);
	allocator.pushSysArgDp(1,&dp7);
	allocator.pushSysArgDp(2,&dp8);
	allocator.pushSysArgDp(3,&dp9);
	allocator.pushSysArgDp(4,&dp10);

	PlnDataPlace* dp;
	dp = allocator.popSysArgDp(4);
	REQUIRE(dp == &dp10);
	REQUIRE(dp->index == R10);
	REQUIRE(dp->place_index == R10);

	dp = allocator.popSysArgDp(3);
	REQUIRE(dp == &dp9);
	REQUIRE(dp->index == RDX);
	REQUIRE(dp->place_index == RDX);

	dp = allocator.popSysArgDp(2);
	REQUIRE(dp == &dp8);
	REQUIRE(dp->index == RSI);
	REQUIRE(dp->place_index == RSI);

	dp = allocator.popSysArgDp(1);
	REQUIRE(dp == &dp7);
	REQUIRE(dp->index == RDI);
	REQUIRE(dp->place_index == RDI);

	dp = allocator.popSysArgDp(6);
	REQUIRE(dp == &dp6);
	REQUIRE(dp->index == R9);
	REQUIRE(dp->place_index == R9);

	dp = allocator.popSysArgDp(5);
	REQUIRE(dp == &dp5);
	REQUIRE(dp->index == R8);
	REQUIRE(dp->place_index == R8);

	dp = allocator.popSysArgDp(4);
	REQUIRE(dp == &dp4);
	REQUIRE(dp->type == DP_TEMP_STK);
	REQUIRE(dp->index == 2);
	REQUIRE(dp->place_index == R10);

	dp = allocator.popSysArgDp(3);
	REQUIRE(dp == &dp3);
	REQUIRE(dp->type == DP_TEMP_STK);
	REQUIRE(dp->index == 1);
	REQUIRE(dp->place_index == RDX);

	dp = allocator.popSysArgDp(2);
	REQUIRE(dp == &dp2);
	REQUIRE(dp->index == R11);
	REQUIRE(dp->place_index == RSI);

	dp = allocator.popSysArgDp(1);
	REQUIRE(dp == &dp1);
	REQUIRE(dp->index == RCX);
	REQUIRE(dp->place_index == RDI);
}
