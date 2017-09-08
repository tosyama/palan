#include "testBase.h"
#include "../models/PlnStack.h"

TEST_CASE("Stack model register item test", "[stack]")
{
	PlnStack* stk = new PlnStack();
	PlnStackItem itm1(8), itm2(8), itm3(8),
		itm4(8), itm5(8), itm6(8), itm7(8);

	//   1 { 2 { 3 } 4 } 5 { 6 } 7
	stk->addItem(&itm1);
	stk->intoBlock();
	stk->addItem(&itm2);
	stk->intoBlock();
	stk->addItem(&itm3);
	REQUIRE(stk->outofBlock() == true);
	stk->addItem(&itm4);
	REQUIRE(stk->outofBlock() == true);
	stk->addItem(&itm5);
	stk->intoBlock();
	stk->addItem(&itm6);
	REQUIRE(stk->outofBlock() == true);
	stk->addItem(&itm7);

	//   *1 { 2 { 3 } 4 } 5 { 6 } 7
	CHECK(stk->items.size() == 1);
	CHECK(stk->items.back() == &itm1);
	//   1 { *2 { 3 } 4 } 5 { 6 } 7
	CHECK(stk->block_stack->items.size() == 1);
	CHECK(stk->block_stack->items.back() == &itm2);
	//   1 { 2 { *3 } 4 } 5 { 6 } 7
	CHECK(stk->block_stack->block_stack->items.size() == 1);
	CHECK(stk->block_stack->block_stack->items.back() == &itm3);
	//   1 { 2 { 3 } *4 } 5 { 6 } 7
	CHECK(stk->block_stack->after_stack->items.size() == 1);
	CHECK(stk->block_stack->after_stack->items.back() == &itm4);
	//   1 { 2 { 3 } 4 } *5 { 6 } 7
	CHECK(stk->after_stack->items.size() == 1);
	CHECK(stk->after_stack->items.back() == &itm5);
	//   1 { 2 { 3 } 4 } 5 { *6 } 7
	CHECK(stk->after_stack->block_stack->items.size() == 1);
	CHECK(stk->after_stack->block_stack->items.back() == &itm6);
	//   1 { 2 { 3 } 4 } 5 { 6 } *7
	CHECK(stk->after_stack->after_stack->items.size() == 1);
	CHECK(stk->after_stack->after_stack->items.back() == &itm7);

	// stack allocation
	//  8: 1
	// 16: 2 5
	// 24: 3 4 6 7
	stk->allocItems0();
	CHECK(stk->total_size == 24);
	CHECK(itm1.pos_from_base == 8);
	CHECK(itm2.pos_from_base == 16);
	CHECK(itm3.pos_from_base == 24);
	CHECK(itm4.pos_from_base == 24);
	CHECK(itm5.pos_from_base == 16);
	CHECK(itm6.pos_from_base == 24);
	CHECK(itm7.pos_from_base == 24);

	delete stk;
}

TEST_CASE("Nomalize stack tree", "[stack]")
{
	PlnStack* stk = new PlnStack();
	PlnStackItem itm1(1),
		itm2(8), itm3(1), itm4(1),
		itm5(8), itm6(1), itm7(4);

	// before ()-stack {}-block N-NULL
	// (1 ({ 2 3 ({}) ({4}) })(5 6 ({7})))
	// after
	// (1 ({ 2 3 4 })(5 6 7))
	stk->addItem(&itm1);
	stk->intoBlock();
	stk->addItem(&itm2);
	stk->addItem(&itm3);
	stk->intoBlock();
	stk->outofBlock();
	stk->intoBlock();
	stk->addItem(&itm4);
	stk->outofBlock();
	stk->outofBlock();
	stk->addItem(&itm5);
	stk->addItem(&itm6);
	stk->intoBlock();
	stk->addItem(&itm7);
	stk->outofBlock();

	stk->normalize();
	CHECK(stk->items.size() == 1);
	CHECK(stk->block_stack->items.size() == 3);
	CHECK(stk->block_stack->block_stack == NULL);
	CHECK(stk->block_stack->after_stack == NULL);
	CHECK(stk->after_stack->items.size() == 3);
	CHECK(stk->after_stack->block_stack == NULL);
	CHECK(stk->after_stack->after_stack == NULL);

	delete stk;
}

TEST_CASE("Optimize position lv1 for saving memory", "[stack]")
{
	PlnStack* stk = new PlnStack();
	PlnStackItem itm1(8), itm2(1), itm3(4),
		itm4(4), itm5(8), itm6(4),
		itm7(8), itm8(1), itm9(4);

	//   1 2 3 { 4 5 6 } 7 8 9
	stk->addItem(&itm1);
	stk->addItem(&itm2);
	stk->addItem(&itm3);
	stk->intoBlock();
	stk->addItem(&itm4);
	stk->addItem(&itm5);
	stk->addItem(&itm6);
	stk->outofBlock();
	stk->addItem(&itm7);
	stk->addItem(&itm8);
	stk->addItem(&itm9);

	// lv0
	stk->allocItems0();
	CHECK(stk->total_size == 36);
	CHECK(itm1.pos_from_base == 8);
	CHECK(itm2.pos_from_base == 9);
	CHECK(itm3.pos_from_base == 16);

	CHECK(itm4.pos_from_base == 20);
	CHECK(itm5.pos_from_base == 32);
	CHECK(itm6.pos_from_base == 36);

	CHECK(itm7.pos_from_base == 24);
	CHECK(itm8.pos_from_base == 25);
	CHECK(itm9.pos_from_base == 32);

	// optimize lv1 (sort & fillin aline)
	stk->allocItems1();
	CHECK(stk->total_size == 32);
	CHECK(itm1.pos_from_base == 8);
	CHECK(itm3.pos_from_base == 12);
	CHECK(itm2.pos_from_base == 13);

	CHECK(stk->block_stack->align_space == 3);
	CHECK(itm5.pos_from_base == 24);
	CHECK(itm4.pos_from_base == 28);
	CHECK(itm6.pos_from_base == 32);

	CHECK(stk->after_stack->align_space == 2);
	CHECK(itm8.pos_from_base == 15);
	CHECK(itm7.pos_from_base == 24);
	CHECK(itm9.pos_from_base == 28);

	delete stk;
}

