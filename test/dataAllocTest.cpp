#include "testBase.h"
#include "../generators/PlnX86_64DataAllocator.h"

TEST_CASE("Register/stack allocation basic test.(Normal call)", "[allocate]")
{
	PlnX86_64DataAllocator x64allocator;
	PlnDataAllocator& allocator = x64allocator;

	PlnDataPlace* dp1=allocator.allocData(8);
	REQUIRE(allocator.data_stack.size() == 1);

	PlnDataPlace* dp2=allocator.allocData(8);
	REQUIRE(allocator.data_stack.size() == 2);
	allocator.releaseData(dp2);

	PlnDataPlace* dp3=allocator.allocData(8);
	REQUIRE(allocator.data_stack.size() == 2);

	vector<PlnParameter*> params(6);
	vector<PlnVariable*> rets;
	auto dps1 = allocator.prepareArgDps(rets.size(), params.size(), DPF_SYS, false);
	for (auto dp: dps1)
		allocator.allocDp(dp);
	REQUIRE(dps1.size() == 6);
	REQUIRE(allocator.data_stack.size() == 2);
	REQUIRE(allocator.arg_stack.size() == 0);

	rets.resize(2);
	params.resize(7);
	auto dps2 = allocator.prepareArgDps(rets.size(), params.size(), DPF_PLN, false);
	for (auto dp: dps2)
		allocator.allocDp(dp);
	REQUIRE(dps2.size() == 7);
	REQUIRE(allocator.data_stack.size() == 6);
	REQUIRE(allocator.arg_stack.size() == 2);
	allocator.funcCalled(dps2, rets, DPF_PLN);
	REQUIRE(allocator.regs[RSI] == dps2[0]);
	REQUIRE(dps2[0]->status==DS_RELEASED);
	REQUIRE(dps2[6]->status==DS_RELEASED);

	allocator.funcCalled(dps1, rets, DPF_SYS);
	REQUIRE(allocator.regs[RDI]->status == DS_RELEASED);
	REQUIRE(dps1[0]->status==DS_RELEASED);
	REQUIRE(dps1[5]->status==DS_RELEASED);

	allocator.finish();

	CHECK(dp1->data.stack.offset == -8);	
	CHECK(dp2->data.stack.offset == -16);	
	CHECK(dp3->data.stack.offset == -16);	
	CHECK(dps2[5]->data.stack.offset == 0);	
	CHECK(dps2[6]->data.stack.offset == 8);	
	CHECK(dps1[0]->save_place->data.stack.offset == -56);	
	CHECK(dps1[1]->save_place->data.stack.offset == -24);	
	CHECK(dps1[5]->save_place->data.stack.offset == -48);	
	CHECK(allocator.stack_size == 80);
}
