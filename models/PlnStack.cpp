/// Stack class definition.
///
/// Stack class manage allocation and calculate location of variables on stack.
///
/// @file	PlnFunction.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <algorithm>
#include "PlnStack.h"

enum PlnStkState {
	SS_CUR_STACK,
	SS_BLOCK_STACK,
	SS_AFTER_STACK,
	SS_EXIT,
};

PlnStack::PlnStack(PlnStack* parent)
	: state(SS_CUR_STACK), parent(parent), total_size(0),
	align_space(0), 
	block_stack(NULL), after_stack(NULL)
{
}

PlnStack::~PlnStack()
{
	if (block_stack) delete block_stack;
	if (after_stack) delete after_stack;
}

void PlnStack::addItem(PlnStackItem* item)
{
	switch (state) {
		case SS_CUR_STACK:
			items.push_back(item);
			break;
		case SS_BLOCK_STACK:
			block_stack->addItem(item);
			break;
		case SS_AFTER_STACK:
			after_stack->addItem(item);
			break;
	}
}

void PlnStack::intoBlock()
{
	switch (state) {
		case SS_CUR_STACK:
			block_stack = new PlnStack(this);
			state = SS_BLOCK_STACK;
			break;
		case SS_BLOCK_STACK:
			block_stack->intoBlock();
			break;
		case SS_AFTER_STACK:
			after_stack->intoBlock();
			break;
	}
}

bool PlnStack::outofBlock()
{
	switch (state) {
		case SS_CUR_STACK:
			return false;

		case SS_BLOCK_STACK:
			if (!block_stack->outofBlock()) {
				after_stack = new PlnStack(this);
				state = SS_AFTER_STACK;
			}
			return true;

		case SS_AFTER_STACK:
			return after_stack->outofBlock();

		default:
			BOOST_ASSERT(false);
	}
}

// Unifiy stacks that can't share memory.
void PlnStack::normalize()
{
	if (block_stack) {
		block_stack->normalize();
		if (block_stack->items.size() == 0
			&& !block_stack->block_stack && !block_stack->after_stack) {
			delete block_stack;
			block_stack = NULL;
		}
	}
	if (after_stack) {
		after_stack->normalize();
		if (after_stack->items.size() == 0
			&& !after_stack->block_stack && !after_stack->after_stack) {
			delete after_stack;
			after_stack = NULL;
		}
	}

	PlnStack* s=NULL;
	if (!block_stack && after_stack)
		s = after_stack;
	else if (block_stack && !after_stack)
		s = block_stack;
	
	if (s) {
		items.insert(items.end(), s->items.begin(), s->items.end());
		block_stack = s->block_stack;
		after_stack = s->after_stack;
		s->block_stack = NULL;
		s->after_stack = NULL;
		delete s;
	}
}

void PlnStack::allocItems0(int base)
{
	int child_base = base;
	int cur_size = 0;
	for (auto i: items) {
		BOOST_ASSERT(!(i->item_size % 2) || i->item_size == 1);
		if (child_base % i->item_size)
			cur_size += i->item_size - (child_base % i->item_size);
		cur_size += i->item_size;	
		child_base = base + cur_size;
		i->pos_from_base = child_base;
	}

	int bsize = 0, asize = 0;
	if (block_stack) {
		block_stack->allocItems0(child_base);
		bsize = block_stack->total_size;
	}
	if (after_stack) {
		after_stack->allocItems0(child_base);
		asize = after_stack->total_size;
	}

	if (items.size())
		align_space = items.front()->pos_from_base - base - items.front()->item_size;
	
	total_size = (bsize > asize) ? bsize + cur_size : asize + cur_size;
}

static bool SIGreater(PlnStackItem* i1, PlnStackItem* i2)
{
	return (i1->item_size) > (i2->item_size);
}

static void move2front(vector<PlnStackItem*>& items, int index)
{
	PlnStackItem* target = items[index];
	for (int i=index; i>0; --i)
		items[i-1] = items[i];
	
	items[0] = target;
}

void PlnStack::allocItems1(int base)
{
	std::stable_sort(items.begin(), items.end(), SIGreater);
	// calculate position of items.
	int cur_size = 0;
	if (items.size()) {
		PlnStackItem* itm=items.front();

		if (base % itm->item_size)
			align_space = itm->item_size - (base % itm->item_size);
		else
			align_space = 0;

		cur_size = align_space + itm->item_size;
		itm->pos_from_base = base + cur_size;

		for (int i=1; i<items.size(); ++i) {
			itm = items[i];
			if (itm->item_size <= align_space) {
				itm->pos_from_base = items.front()->pos_from_base
						- items.front()->item_size - itm->item_size;
				align_space -= itm->item_size;
				move2front(items, i);
			} else {
				cur_size += itm->item_size;
				itm->pos_from_base = base + cur_size;
			}
		}
	}

	base += cur_size;

	// calculate total stack size.
	int bsize = 0, asize = 0;
	if (block_stack) {
		block_stack->allocItems1(base);
		bsize = block_stack->total_size;
	}
	if (after_stack) {
		after_stack->allocItems1(base);
		asize = after_stack->total_size;
	}
	
	total_size = (bsize > asize) ? bsize + cur_size : asize + cur_size;
}

PlnStackItem::PlnStackItem(int item_size)
	: item_size(item_size), pos_from_base(0)
{
}

