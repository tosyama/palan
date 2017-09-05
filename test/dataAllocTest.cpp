#include "testBase.h"
#include "../generators/PlnX86_64DataAllocator.h"

TEST_CASE("Register allocation basic test.(Normal call)", "[allocate]")
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

	vector<PlnParameter*> params(7);
	vector<PlnVariable*> rets;
	vector<PlnDataPlace*> dps1 = allocator.allocArgs(params, rets);
	REQUIRE(dps1.size() == 7);
	REQUIRE(allocator.data_stack.size() == 2);
	REQUIRE(allocator.arg_stack.size() == 1);

	rets.resize(2);
	vector<PlnDataPlace*> dps2 = allocator.allocArgs(params, rets);
	REQUIRE(dps2.size() == 7);
	REQUIRE(allocator.data_stack.size() == 8);
	REQUIRE(allocator.arg_stack.size() == 2);
	allocator.funcCalled(dps2, rets);
	REQUIRE(allocator.regs[RDX]->status == DS_RELEASED);
	REQUIRE(dps2[0]->status==DS_RELEASED);
	REQUIRE(dps2[6]->status==DS_RELEASED);

	allocator.funcCalled(dps1, rets);
	REQUIRE(allocator.regs[RDX]->status == DS_DESTROYED);
	REQUIRE(dps1[0]->status==DS_RELEASED);
	REQUIRE(dps1[6]->status==DS_RELEASED);
}
