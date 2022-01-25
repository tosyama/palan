/// Structure type class definition.
///
/// @file	PlnStructType.cpp
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

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
#include "../expressions/PlnArrayValue.h"
#include "PlnStructType.h"
#include "PlnArrayValueType.h"
#include "PlnFixedArrayType.h"

PlnStructMemberDef::PlnStructMemberDef(PlnVarType *type, const string& name)
	: type(type), name(name), offset(0)
{
}

static PlnFunction* createObjMemberStructAllocFunc(const string& func_name, PlnStructTypeInfo* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);

	f->parent = parent_block;
	string s1 = "__p1";
	PlnVariable* ret_var = f->addRetValue(s1, struct_type->getVarType("wcr"));

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	palan::malloc(f->implement, ret_var, new PlnExpression(uint64_t(struct_type->data_size)));

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type() == DT_OBJECT_REF) {
			PlnValue var_val(ret_var);

			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			member_ex->values[0].asgn_type = ASGN_COPY_REF;
			vector<PlnExpression*> lvals = { member_ex };

			vector<PlnExpression*> args;
			mdef->type->getInitExpressions(args);
			PlnExpression* alloc_ex = mdef->type->getAllocEx(args);
			vector<PlnExpression*> exps = { alloc_ex };

			auto assign = new PlnAssignment(lvals, exps);
			block->statements.push_back(new PlnStatement(assign, block));

		} else if (mdef->type->data_type() == DT_OBJECT) {
			auto struct_ex = new PlnExpression(ret_var);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			PlnExpression* internal_alloc_ex = mdef->type->getInternalAllocEx(member_ex);
			if (internal_alloc_ex) {
				block->statements.push_back(new PlnStatement(internal_alloc_ex, block));
			}
		}
	}

	return f;
}

static PlnFunction* createInternalObjMemberStructAllocFunc(const string& func_name, PlnStructTypeInfo* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);

	f->parent = parent_block;
	string s1 = "__p1";
	f->addParam(s1, struct_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type() == DT_OBJECT_REF) {
			PlnValue var_val(f->parameters[0]->var);
			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			member_ex->values[0].asgn_type = ASGN_COPY_REF;
			vector<PlnExpression*> lvals = { member_ex };

			vector<PlnExpression*> args;
			mdef->type->getInitExpressions(args);
			PlnExpression* alloc_ex = mdef->type->getAllocEx(args);
			vector<PlnExpression*> exps = { alloc_ex };

			auto assign = new PlnAssignment(lvals, exps);
			block->statements.push_back(new PlnStatement(assign, block));

		} else if (mdef->type->data_type() == DT_OBJECT) {
			PlnValue var_val(f->parameters[0]->var);
			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			PlnExpression* internal_alloc_ex = mdef->type->getInternalAllocEx(member_ex);
			if (internal_alloc_ex) {
				block->statements.push_back(new PlnStatement(internal_alloc_ex, block));
			}
		}
	}

	return f;
}

static PlnFunction* createObjMemberStructFreeFunc(const string& func_name, PlnStructTypeInfo* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1";
	f->addParam(s1, struct_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);
	f->parent = parent_block;

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	// Return if object address is 0.
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(f->parameters[0]->var), ifblock, NULL, f->implement);
	block->statements.push_back(if_obj);

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type() == DT_OBJECT_REF) {
			PlnValue var_val(f->parameters[0]->var);
			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			PlnExpression* free_member = mdef->type->getFreeEx(member_ex);
			ifblock->statements.push_back(new PlnStatement(free_member, block));
		}
	}

	palan::free(ifblock, f->parameters[0]->var);

	return f;
}

static PlnFunction* createObjMemberStructInternalFreeFunc(const string& func_name, PlnStructTypeInfo* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1";
	f->addParam(s1, struct_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);
	f->parent = parent_block;

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;

	for (PlnStructMemberDef* mdef: struct_type->members) {
		if (mdef->type->data_type() == DT_OBJECT_REF) {
			PlnValue var_val(f->parameters[0]->var);
			auto struct_ex = new PlnExpression(var_val);
			auto member_ex = new PlnStructMember(struct_ex, mdef->name);
			PlnExpression* free_member = mdef->type->getFreeEx(member_ex);
			block->statements.push_back(new PlnStatement(free_member, block));
		}
	}

	return f;
}

static PlnFunction* createObjMemberStructCopyFunc(const string& func_name, PlnStructTypeInfo* struct_type, PlnBlock* parent_block)
{
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1", s2 = "__p2";
	// dst
	PlnVariable* dst_var = f->addParam(s1, struct_type->getVarType("wcr"), PIO_INPUT, FPM_IN_BYREF, NULL);
	// src
	PlnVariable* src_var = f->addParam(s2, struct_type->getVarType("rir"), PIO_INPUT, FPM_IN_BYREF, NULL);
	f->parent = parent_block;

	auto block = new PlnBlock();
	block->setParent(f);
	f->implement = block;
	
	for (PlnStructMemberDef* mdef: struct_type->members) {
			auto dst_struct_ex = new PlnExpression(dst_var);
			auto dst_member_ex = new PlnStructMember(dst_struct_ex, mdef->name);

			auto src_struct_ex = new PlnExpression(src_var);
			auto src_member_ex = new PlnStructMember(src_struct_ex, mdef->name);
		if (mdef->type->data_type() == DT_OBJECT_REF || mdef->type->data_type() == DT_OBJECT) {
			PlnExpression* copy_member = mdef->type->getCopyEx(dst_member_ex, src_member_ex);
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

PlnStructTypeInfo::PlnStructTypeInfo(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent, const string& default_mode)
	: PlnTypeInfo(TP_STRUCT), members(move(members))
{
	this->name = name;
	int alloc_size = 0;
	int max_member_align = 1;
	this->default_mode = default_mode;
	has_heap_member = false;
	bool need_alloc_func = false;

	for (auto m: this->members) {
		int member_size = m->type->size();
		int member_align = m->type->align();

		// padding
		if (alloc_size % member_align) {
			alloc_size = (alloc_size / member_align+1) * member_align;
		}

		m->offset = alloc_size;

		if (member_align > max_member_align) {
			max_member_align = member_align;
		}
		alloc_size += m->type->size();

		if (m->type->mode[ALLOC_MD] == 'h'
				|| m->type->has_heap_member()) {
			need_alloc_func = true;
			has_heap_member = true;
		}
	}

	// last padding
	if (alloc_size % max_member_align) {
		alloc_size = (alloc_size / max_member_align+1) * max_member_align;
	}
	
	data_align = max_member_align;

	data_type = DT_OBJECT;
	data_size = alloc_size;

	if (need_alloc_func) {
		string fname = PlnBlock::generateFuncName("new", {this}, {});
		PlnFunction *alloc_func = createObjMemberStructAllocFunc(fname, this, parent);
		allocator = new PlnNoParamAllocator(alloc_func);

		fname = PlnBlock::generateFuncName("init", {}, {this});
		PlnFunction *internal_alloc_func = createInternalObjMemberStructAllocFunc(fname, this, parent);
		internal_allocator = new PlnSingleParamInternalAllocator(internal_alloc_func);

		fname = PlnBlock::generateFuncName("copy", {}, {this,this});
		PlnFunction *copy_func = createObjMemberStructCopyFunc(fname, this, parent);
		copyer = new PlnTwoParamsCopyer(copy_func);

		fname = PlnBlock::generateFuncName("free", {}, {this});
		PlnFunction *free_func = createObjMemberStructFreeFunc(fname, this, parent);
		freer = new PlnSingleParamFreer(free_func);

		fname = PlnBlock::generateFuncName("intrfree", {}, {this});
		PlnFunction *internal_free_func = createObjMemberStructInternalFreeFunc(fname, this, parent);
		internal_freer = new PlnSingleParamFreer(internal_free_func);

		parent->parent_module->functions.push_back(alloc_func);
		parent->parent_module->functions.push_back(internal_alloc_func);
		parent->parent_module->functions.push_back(copy_func);
		parent->parent_module->functions.push_back(free_func);
		parent->parent_module->functions.push_back(internal_free_func);

	} else {
		allocator = new PlnSingleObjectAllocator(alloc_size);
		copyer = new PlnSingleObjectCopyer(alloc_size);
		freer = new PlnSingleObjectFreer();
	}
}

PlnStructTypeInfo::~PlnStructTypeInfo()
{
	for (auto member: members)
		delete member;
}

PlnTypeConvCap PlnStructTypeInfo::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) {
	if (this == src->typeinf)
		return TC_SAME;

	if (src->typeinf == PlnVarType::getObject()->typeinf) {
		return TC_DOWN_CAST;
	}

	if (src->typeinf->type == TP_ARRAY_VALUE) {
		PlnArrayValue* arr_val = static_cast<PlnArrayValueTypeInfo*>(src->typeinf)->arr_val;

		if (arr_val->item_exps.size() != members.size())
			return TC_CANT_CONV;

		PlnTypeConvCap cap = TC_SAME;
		int i = 0;
		for (auto member: members) {
			PlnVarType* src_type = arr_val->item_exps[i]->values[0].getVarType();
			cap = PlnTypeInfo::lowCapacity(cap, member->type->canCopyFrom(src_type, ASGN_COPY));
			i++;
		}

		return cap;
	}
	
	return TC_CANT_CONV;
}

