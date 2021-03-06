#include <iostream>
#include <climits>
#include "testBase.h"

#include "../generators/PlnX86_64DataAllocator.h"
#include "../PlnConstants.h"

TEST_CASE("Register/stack allocation basic test.(Normal call)", "[allocate]")
{
	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& allocator = x64allocator;

	PlnDataPlace* dp1=allocator.allocData(8, DT_SINT);
	REQUIRE(allocator.data_stack.size() == 1);	// stack[dp1]

	PlnDataPlace* dp2=allocator.allocData(8, DT_SINT);
	REQUIRE(allocator.data_stack.size() == 2);	// stack[dp1 dp2]
	allocator.releaseDp(dp2);	// stack[dp1, *]	

	PlnDataPlace* dp3=allocator.allocData(8, DT_SINT);
	REQUIRE(allocator.data_stack.size() == 2);	// stack[dp1 dp3]

	vector<PlnParameter*> params(6);
	vector<int> adtypes = { DT_SINT, DT_SINT, DT_SINT, DT_SINT, DT_SINT, DT_SINT };
	vector<PlnVariable*> rets;

	vector<PlnDataPlace*> dps1;
	for (int t: adtypes)
		dps1.push_back(new PlnDataPlace(8, t));
	allocator.setArgDps(FT_SYS, dps1, false);
	for (auto dp: dps1)
		allocator.allocDp(dp);
	REQUIRE(dps1.size() == 6);
	REQUIRE(allocator.data_stack.size() == 2);	// stack[dp1 dp3]
	REQUIRE(allocator.arg_stack.size() == 0);

	rets.resize(2);
	params.resize(7);
	adtypes.push_back( DT_SINT );
	vector<PlnDataPlace*> dps2;
	for (int t: adtypes)
		dps2.push_back(new PlnDataPlace(8, t));
	allocator.setArgDps(FT_SYS, dps2, false);
	for (auto dp: dps2)
		allocator.allocDp(dp);	// stack[dp dp3 dp1save1 .. dp1save6 dp2arg1]	// ap1arg4 is not save
	REQUIRE(dps2.size() == 7);
	REQUIRE(allocator.data_stack.size() == 2);
	REQUIRE(allocator.arg_stack.size() == 1);
	allocator.funcCalled(dps2, FT_PLN, false);
	REQUIRE(allocator.regs[RDI] == dps2[0]);
	REQUIRE(dps2[0]->status==DS_RELEASED);
	REQUIRE(dps2[6]->status==DS_RELEASED);

	allocator.funcCalled(dps1, FT_SYS, false);
	REQUIRE(allocator.regs[RDI]->status == DS_RELEASED);
	REQUIRE(dps1[0]->status==DS_RELEASED);
	REQUIRE(dps1[5]->status==DS_RELEASED);

	allocator.finish();

	CHECK(dp1->data.stack.offset == -8);	
	CHECK(dp2->data.stack.offset == -16);	
	CHECK(dp3->data.stack.offset == -16);	
	CHECK(dps2[6]->data.stack.offset == 0);	

	CHECK(dps1[0]->save_place == NULL);	
	CHECK(dps1[1]->save_place == NULL);	
	CHECK(dps1[5]->save_place == NULL);	
	CHECK(allocator.stack_size == 24);
}

TEST_CASE("Mixed bytes dateplace allocate unit test.", "[allocate]")
{
	PlnDataPlace dp_ctnr(8, DT_UNKNOWN);
	vector<PlnDataPlace*> bdps;
	dp_ctnr.type = DP_BYTES;
	dp_ctnr.data.bytesData = &bdps;
	dp_ctnr.status = DS_ASSIGNED_SOME;
	dp_ctnr.alloc_step = INT_MAX;
	dp_ctnr.release_step = 0;

	PlnDataPlace dp1(4, DT_SINT);
	dp1.alloc_step = 1;
	dp1.release_step = INT_MAX;
	dp1.status = DS_ASSIGNED;

	REQUIRE(dp_ctnr.tryAllocBytes(&dp1) == true);
	REQUIRE(bdps.size() == 1);
	REQUIRE(bdps[0] == &dp1);

	PlnDataPlace dp2(4, DT_SINT);
	dp2.alloc_step = 5;
	dp2.release_step = INT_MAX;
	dp2.status = DS_ASSIGNED;

	REQUIRE(dp_ctnr.tryAllocBytes(&dp2) == true);
	REQUIRE(bdps.size() == 2);

	// failed to alloc because all bytes using.
	PlnDataPlace dp3(2, DT_SINT);
	dp3.alloc_step = 1;
	dp3.release_step = INT_MAX;
	dp3.status = DS_ASSIGNED;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp3) == false);
	REQUIRE(bdps.size() == 2);

	// release dp1
	dp1.release_step = 10;
	dp1.status = DS_RELEASED;

	// try again but will fail because steps overlaped.
	dp3.alloc_step = 10;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp3) == false);
	REQUIRE(bdps.size() == 2);

	// try again
	dp3.alloc_step = 11;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp3) == true);
	REQUIRE(bdps.size() == 3);

	// alloc another 2bytes
	PlnDataPlace dp4(2, DT_SINT);
	dp4.alloc_step = 10;
	dp4.release_step = INT_MAX;
	dp4.status = DS_ASSIGNED;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp4) == false);
	dp4.alloc_step = 11;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp4) == true);
	REQUIRE(bdps.size() == 4);

	// alloc past.
	PlnDataPlace dp5(1, DT_SINT);
	dp5.alloc_step = 2;
	dp5.release_step = 4;
	dp5.status = DS_RELEASED;
	REQUIRE(dp_ctnr.tryAllocBytes(&dp5) == true);
	REQUIRE(bdps.size() == 5);

	// release all
	dp2.release_step = 12;
	dp2.status = DS_RELEASED;
	dp3.release_step = 15;
	dp3.status = DS_RELEASED;
	dp4.release_step = 14;
	dp4.status = DS_RELEASED;

	dp_ctnr.updateBytesDpStatus();	
	REQUIRE(dp_ctnr.status == DS_RELEASED);
	REQUIRE(dp_ctnr.alloc_step == 1);
	REQUIRE(dp_ctnr.release_step == 15);
}

TEST_CASE("Mixed bytes allocation test.", "[allocate]")
{
	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& allocator = x64allocator;

	auto dp1=allocator.allocData(1, DT_SINT);
	auto dp2=allocator.allocData(2, DT_SINT);
	auto dp3=allocator.allocData(4, DT_SINT);
	REQUIRE(allocator.data_stack.size() == 1);

	auto dp4=allocator.allocData(2, DT_SINT);
	REQUIRE(allocator.data_stack.size() == 2);

	auto dp5=allocator.allocData(1, DT_SINT);
	allocator.releaseDp(dp2);
	auto dp6=allocator.allocData(2, DT_SINT);
	auto dp7=allocator.allocData(4, DT_SINT);

	allocator.finish();

	CHECK(allocator.stack_size == 16);
	CHECK(dp1->data.stack.offset == -8);	
	CHECK(dp2->data.stack.offset == -6);	
	CHECK(dp3->data.stack.offset == -4);	
	CHECK(dp4->data.stack.offset == -16);	
	CHECK(dp5->data.stack.offset == -7);	
	CHECK(dp6->data.stack.offset == -6);	
	CHECK(dp7->data.stack.offset == -12);	
}

TEST_CASE("Data source and save management.", "[allocate]")
{
	// [Design Memo]
	// pushSrc: Allocate source data if needed.
	// popSrc: Release source data and allocate destination data if needed.

	// src: keep, dst: keep  ->  move when popSrc.
	// src: keep, dst: destroy  ->  move when popSrc.
	// src: destroy, dst: keep -> move when pushSrc by allocating and setting dst to save_place.
	// src: destroy, dst: destroy -> alloc save_place and save src when pushSrc.
	//								load from save_place and release when popSrc.

	// Pattern
	// src: Reg(work/get return), dst Reg(work/param/set return)
	// src: Stack(variable), dst Reg(work/param)
	// src: Stack(variable), dst Stack(variable)
	
	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& da = x64allocator;

	int push_step, pop_step;

	// src(Reg(accumlateor)): keep, dst(Reg): keep
	{
		auto dp_ac_dst = da.prepareAccumulator(DT_SINT, 8);
		auto dp_ac_src = da.prepareAccumulator(DT_SINT, 8);
		da.allocDp(dp_ac_src);

		push_step = da.step;
		da.pushSrc(dp_ac_dst, dp_ac_src);
		CHECK(dp_ac_src->alloc_step <= push_step);
		CHECK(dp_ac_src->release_step > 999999);
		CHECK(dp_ac_src->status == DS_ASSIGNED);
		CHECK(dp_ac_dst->status != DS_ASSIGNED);

		pop_step = da.step;
		da.popSrc(dp_ac_dst);

		// exchage data when pop.
		CHECK(dp_ac_src->alloc_step <= push_step);
		CHECK(dp_ac_src->release_step == pop_step);
		CHECK(dp_ac_src->save_place == NULL);
		CHECK(dp_ac_src->status == DS_RELEASED);

		CHECK(dp_ac_dst->status == DS_ASSIGNED);
		CHECK(dp_ac_dst->save_place == NULL);
		CHECK(dp_ac_dst->alloc_step == (pop_step+1));

		da.releaseDp(dp_ac_dst);
	}

	// src(Reg): keep, dst(Reg): destroy
	{
		auto dp_ac_src = da.prepareAccumulator(DT_SINT, 8);
		da.allocDp(dp_ac_src);
		vector<PlnDataPlace*> dst_arg1 = { new PlnDataPlace(8, DT_SINT) };
		dst_arg1[0]->status = DS_READY_ASSIGN;
		da.setArgDps(FT_PLN, dst_arg1, false);

		push_step = da.step;
		da.pushSrc(dst_arg1[0], dp_ac_src);

		// destory
		vector<PlnDataPlace*> dst_arg2= { new PlnDataPlace(8, DT_SINT) };
		da.setArgDps(FT_PLN, dst_arg2, false);
		da.allocDp(dst_arg2[0]);
		da.releaseDp(dst_arg2[0]);

		pop_step = da.step;
		da.popSrc(dst_arg1[0]);

		// exchage data when pop.
		CHECK(dp_ac_src->alloc_step <= push_step);
		CHECK(dp_ac_src->release_step == pop_step);
		CHECK(dp_ac_src->status == DS_RELEASED);

		CHECK(dst_arg2[0]->status == DS_RELEASED);
		CHECK(dst_arg2[0]->alloc_step > push_step);
		CHECK(dst_arg2[0]->release_step < pop_step);

		CHECK(dst_arg1[0]->status == DS_ASSIGNED);
		CHECK(dst_arg1[0]->save_place == NULL);
		CHECK(dst_arg1[0]->alloc_step == (pop_step+1));

		da.releaseDp(dst_arg1[0]);
	}

	// src(Reg): destory, dst(Reg): keep
	{
		auto dp_ac_src = da.prepareAccumulator(DT_SINT, 8);
		da.allocDp(dp_ac_src);
		vector<PlnDataPlace*> dst_arg = { new PlnDataPlace(8, DT_SINT) };
		dst_arg[0]->status = DS_READY_ASSIGN;
		da.setArgDps(FT_PLN, dst_arg, false);

		push_step = da.step;
		da.pushSrc(dst_arg[0], dp_ac_src);

		auto dp_ac_src2 = da.prepareAccumulator(DT_SINT, 8);
		da.allocDp(dp_ac_src2);
		da.releaseDp(dp_ac_src2);

		pop_step = da.step;
		da.popSrc(dst_arg[0]);

		// exchage data when push.
		CHECK(dp_ac_src->alloc_step <= push_step);
		CHECK(dp_ac_src->release_step == push_step);
		CHECK(dp_ac_src->status == DS_RELEASED);

		CHECK(dst_arg[0]->status == DS_ASSIGNED);
		CHECK(dst_arg[0]->save_place == dst_arg[0]);
		CHECK(dst_arg[0]->alloc_step == push_step);
		da.releaseDp(dst_arg[0]);
	}
//--- 
	auto dpv4 = da.allocData(4, DT_SINT);
	vector<PlnDataPlace*> dp_prms = { new PlnDataPlace(8, DT_SINT), new PlnDataPlace(8, DT_SINT), new PlnDataPlace(8, DT_SINT)};
	da.setArgDps(FT_PLN, dp_prms, false);

	da.pushSrc(dp_prms[0], dpv4);
	da.popSrc(dp_prms[0]);

	for (auto dp: dp_prms)
		da.allocDp(dp);

	da.funcCalled(dp_prms, FT_PLN, false);

	da.finish();
}
