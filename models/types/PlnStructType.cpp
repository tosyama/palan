/// Structure type class definition.
///
/// @file	PlnStructType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnModel.h"
#include "../../PlnConstants.h"
#include "../../PlnTreeBuildHelper.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../PlnGeneralObject.h"
#include "../PlnFunction.h"
#include "../PlnBlock.h"
#include "../PlnModule.h"
#include "../PlnStatement.h"
#include "../PlnVariable.h"
#include "../PlnConditionalBranch.h"
#include "../expressions/PlnStructMember.h"
#include "../expressions/PlnAssignment.h"
#include "../expressions/PlnMemCopy.h"
#include "PlnStructType.h"

PlnStructMemberDef::PlnStructMemberDef(PlnType *type, string name)
	: type(type), name(name), offset(0)
{
}

static PlnFunction* createObjMemberStructAllocFunc(const string func_name, PlnStructType* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);

	f->parent = parent_block;
	string s1 = "__p1";
	PlnVariable* ret_var = f->addRetValue(s1, struct_type, false, false);

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	palan::malloc(f->implement, ret_var, struct_type->inf.obj.alloc_size);

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type == DT_OBJECT_REF) {
			PlnValue var_val(ret_var);

			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			member_ex->values[0].asgn_type = ASGN_COPY_REF;
			vector<PlnExpression*> lvals = { member_ex };

			PlnExpression* alloc_ex = mdef->type->allocator->getAllocEx();
			vector<PlnExpression*> exps = { alloc_ex };

			auto assign = new PlnAssignment(lvals, exps);
			block->statements.push_back(new PlnStatement(assign, block));
		}
	}

	return f;
}

static PlnFunction* createObjMemberStructFreeFunc(const string func_name, PlnStructType* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1";
	f->addParam(s1, struct_type, PIO_INPUT, FPM_REF, NULL);
	f->parent = parent_block;

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	// Return if object address is 0.
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(f->parameters[0]), ifblock, NULL, f->implement);
	block->statements.push_back(if_obj);

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type == DT_OBJECT_REF) {
			PlnValue var_val(f->parameters[0]);
			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			PlnExpression* free_member = mdef->type->freer->getFreeEx(member_ex);
			ifblock->statements.push_back(new PlnStatement(free_member, block));
		}
	}

	palan::free(ifblock, f->parameters[0]);

	return f;
}

static PlnFunction* createObjMemberStructCopyFunc(const string func_name, PlnStructType* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1", s2 = "__p2";
	// dst
	PlnVariable* dst_var = f->addParam(s1, struct_type, PIO_INPUT, FPM_REF, NULL);
	// src
	PlnVariable* src_var = f->addParam(s2, struct_type, PIO_INPUT, FPM_REF, NULL);
	f->parent = parent_block;

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;
	
	for (PlnStructMemberDef* mdef: struct_type->members) {
			auto dst_struct_ex = new PlnExpression(dst_var);
			auto dst_member_ex = new PlnStructMember(dst_struct_ex, mdef->name);

			auto src_struct_ex = new PlnExpression(src_var);
			auto src_member_ex = new PlnStructMember(src_struct_ex, mdef->name);
		if (mdef->type->data_type == DT_OBJECT_REF) {
			PlnExpression* copy_member = mdef->type->copyer->getCopyEx(dst_member_ex, src_member_ex);
			block->statements.push_back(new PlnStatement(copy_member, block));

		} else {
			vector<PlnExpression*> lvals = { dst_member_ex };
			vector<PlnExpression*> exps = { src_member_ex };

			auto assign = new PlnAssignment(lvals, exps);
			block->statements.push_back(new PlnStatement(assign, block));
		}
	}

	return f;
}

PlnStructType::PlnStructType(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent)
	: PlnType(TP_STRUCT), members(move(members))
{
	this->name = name;
	data_type = DT_OBJECT_REF;
	size = 8;
	int alloc_size = 0;
	int max_member_size = 1;
	bool has_object_member = false;

	for (auto m: this->members) {
		int member_size = m->type->size;
		// padding
		if (alloc_size % member_size) {
			alloc_size = (alloc_size / member_size+1) * member_size;
		}

		m->offset = alloc_size;

		if (member_size > max_member_size) {
			max_member_size = member_size;
		}
		alloc_size += m->type->size;

		if (m->type->data_type == DT_OBJECT_REF) {
			has_object_member = true;
		}
	}

	// last padding
	if (alloc_size % max_member_size) {
		alloc_size = (alloc_size / max_member_size+1) * max_member_size;
	}

	inf.obj.is_fixed_size = true;
	inf.obj.alloc_size = alloc_size;

	if (has_object_member) {
		string fname = PlnBlock::generateFuncName("new", {this}, {});
		PlnFunction *alloc_func = createObjMemberStructAllocFunc(fname, this, parent);
		allocator = new PlnNoParamAllocator(alloc_func);

		fname = PlnBlock::generateFuncName("copy", {}, {this,this});
		PlnFunction *copy_func = createObjMemberStructCopyFunc(fname, this, parent);
		copyer = new PlnTwoParamsCopyer(copy_func);

		fname = PlnBlock::generateFuncName("free", {}, {this});
		PlnFunction *free_func = createObjMemberStructFreeFunc(fname, this, parent);
		freer = new PlnSingleParamFreer(free_func);

		parent->parent_module->functions.push_back(alloc_func);
		parent->parent_module->functions.push_back(copy_func);
		parent->parent_module->functions.push_back(free_func);

	} else {
		allocator = new PlnSingleObjectAllocator(alloc_size);
		copyer = new PlnSingleObjectCopyer(alloc_size);
		freer = new PlnSingleObjectFreer();
	}
}

PlnStructType::~PlnStructType()
{
	for (auto member: members)
		delete member;
}

PlnTypeConvCap PlnStructType::canConvFrom(PlnType *src) {
	if (this == src)
		return TC_SAME;
	
	if (src == PlnType::getObject()) {
		return TC_DOWN_CAST;
	}
	
	return TC_CANT_CONV;
}
