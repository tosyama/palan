/// Stack class declaration
///
/// @file	PlnStack.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnStackItem;
class PlnBlockStackItem;

class PlnStack
{
public:
	int state;
	int total_size;
	int align_space;

	vector<PlnStackItem*> items;
	int args_size;
	PlnStack*	parent;
	PlnStack*	block_stack;
	PlnStack*	after_stack;

	PlnStack(PlnStack* parent = NULL);
	~PlnStack();

	void addItem(PlnStackItem* item);
	void reserveArgs(int size);
	void intoBlock();
	bool outofBlock();

	void normalize();
	void allocItems0(int base = 0);
	void allocItems1(int base = 0);
};

class PlnStackItem
{
public:
	PlnStackItem(int item_size);
	int item_size;
	int pos_from_base;
};

