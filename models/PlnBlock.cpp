/// Block model class definition.
///
/// Block model manage list of statements and local variables.
/// Block: "{" statements "}"
///
/// @file	PlnBlock.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include "PlnFunction.h"
#include "PlnGeneralObject.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnExpression.h"
#include "types/PlnFixedArrayType.h"
#include "types/PlnStructType.h"
#include "types/PlnAliasType.h"
#include "PlnArray.h"
#include "PlnModule.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "../PlnMessage.h"
#include "../PlnException.h"

PlnBlock::PlnBlock()
	: parent_module(NULL), parent_func(NULL), parent_block(NULL), owner_stmt(NULL)
{
}

PlnBlock::~PlnBlock()
{
	for (auto v: variables)
		delete v;
	for (auto free_var: free_vars)
		delete free_var;
	for (auto stmt: statements)
		delete stmt;
	for (auto cons: consts)
		delete cons.ex;
	for (auto t: types)
		delete t;
}

void PlnBlock::setParent(PlnFunction* f)
{
	parent_func = f;
	parent_block = NULL;

	BOOST_ASSERT(f->parent);
	parent_module = f->parent->parent_module;
	BOOST_ASSERT(parent_module);
}

void PlnBlock::setParent(PlnBlock* b)
{
	parent_func = b->parent_func;
	parent_block = b;
	parent_module = b->parent_module;
}

PlnVariable* PlnBlock::declareVariable(const string& var_name, PlnType* var_type, bool readonly, bool is_owner, bool do_check_ancestor_blocks)
{
	for (auto v: variables)
		if (v->name == var_name) return NULL;
	for (auto c: consts)
		if (c.name == var_name) return NULL;
	
	if (parent_func) {
		for (auto rv: parent_func->return_vals)
			if (rv->name == var_name) return NULL;
		for (auto p: parent_func->parameters)
			if (p->name == var_name) return NULL;
	}

	if (do_check_ancestor_blocks) {
		PlnBlock *b = this->parent_block;
		while (b) {
			for (auto v: b->variables)
				if (v->name == var_name) return NULL;
			for (auto c: b->consts)
				if (c.name == var_name) return NULL;
			b = b->parent_block;
		}
	}

	PlnVariable* v = new PlnVariable();
	v->name = var_name;
	v->container = NULL;

	if (!var_type) {
		var_type = variables.back()->var_type2;
	}

	v->var_type2 = var_type;
	v->var_type = var_type->getVarType(var_type->mode);

	if (var_type->mode == "rir") {
		is_owner = false;
		readonly = true;
	}

	if (v->var_type->data_type() == DT_OBJECT_REF) {
		v->ptr_type = PTR_REFERENCE;
		if (is_owner)
			v->ptr_type |= PTR_OWNERSHIP;
	} else
		v->ptr_type = NO_PTR;

	if (readonly)
		v->ptr_type |= PTR_READONLY;

	variables.push_back(v);

	return v;
}

PlnVariable* PlnBlock::getVariable(const string& var_name)
{
	PlnBlock* b = this;
	for(;;) {
		for (auto v: b->variables)
			if (v->name == var_name)
				return v;
		if (b->parent_block)
			b = b->parent_block;
		else if (b->parent_func) {
			for (auto rv: parent_func->return_vals)
				if (rv->name == var_name) return rv;
			for (auto p: parent_func->parameters)
				if (p->name == var_name) return p;
			return NULL;
		} else
			return NULL;
		// TODO: search grobal.
	}
}

static PlnBlock* parentBlock(PlnBlock* block) {
	if (block->parent_block) return block->parent_block;
	if (block->parent_func) return block->parent_func->parent;
	return NULL;
}

void PlnBlock::declareConst(const string& name, PlnExpression *ex)
{
	bool isConst = false;
	if (ex->type == ET_VALUE) {
		int vtype = ex->values[0].type;
		if (vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8 || vtype == VL_LIT_FLO8
			|| vtype == VL_LIT_STR || vtype == VL_LIT_ARRAY)
			isConst = true;
	}

	if (!isConst) {
		PlnCompileError err(E_CantDefineConst, name);
		throw err;
	}

	// Always 0. Const declaration is faster than variable.
	for (auto v: variables)
		BOOST_ASSERT(v->name == name);

	for (auto c: consts)
		if (c.name == name) {
			PlnCompileError err(E_DuplicateConstName, name);
			throw err;
		}

	consts.push_back( {name, ex} );
}

PlnExpression* PlnBlock::getConst(const string& name)
{
	PlnExpression* e;
	PlnBlock* b = this;
	do {
		auto const_inf = find_if(b->consts.begin(), b->consts.end(),
			[name](PlnConst& c) { return c.name == name; } );

		if (const_inf != b->consts.end()) {
			PlnExpression* ex = const_inf->ex;
			if (ex->type == ET_VALUE) {
				return new PlnExpression(ex->values[0]);
			} else
				BOOST_ASSERT(false);
		}

	} while (b = parentBlock(b));
	return NULL;
}

void PlnBlock::declareType(const string& type_name)
{
	PlnType* t = new PlnType();
	t->name = type_name;
	t->mode = "wmo";
	t->default_mode = "wmo";
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	types.push_back(t);
}

void PlnBlock::declareType(const string& type_name, vector<PlnStructMemberDef*> &members)
{
	auto t = new PlnStructType(type_name, members, this, "wmo");
	types.push_back(t);
}

void PlnBlock::declareAliasType(const string& type_name, PlnType* orig_type)
{
	BOOST_ASSERT(orig_type);

	auto t = new PlnAliasType(type_name, orig_type);
	types.push_back(t);
}

static PlnType* realType(PlnType *t, const string& mode) {
	while (t->type == TP_ALIAS) {
		t = static_cast<PlnAliasType*>(t)->orig_type;
	}
	return t->getTypeWithMode(mode);
}

PlnType* PlnBlock::getType(const string& type_name, const string& mode)
{
	// Crrent block
	{
		auto t = std::find_if(types.begin(), types.end(),
				[type_name](PlnType* t) { return t->name == type_name; });

		if (t != types.end())
			return realType(*t, mode);
	}

	// Parent block
	if (PlnBlock* b = parentBlock(this)) {
		return b->getType(type_name, mode);
	}

	// Search default type if toplevel: parentBlock(this) == NULL
	{
		vector<PlnType*> &basic_types = PlnType::getBasicTypes();	

		auto t = std::find_if(basic_types.begin(), basic_types.end(),
				[type_name](PlnType* t) { return t->name == type_name; });

		if (t != basic_types.end())
			return realType(*t, mode);
	}

	return NULL;
}

string PlnBlock::generateFuncName(string fname, vector<PlnType*> ret_types, vector<PlnType*> arg_types)
{
	string ret_name, arg_name;
	for (auto t: ret_types)
		ret_name += "_" + t->name;
	for (auto t: arg_types)
		arg_name += "_" + t->name;
	boost::replace_all(fname, "_", "__");

	fname = "_" + fname + ret_name + + "_" +arg_name;
	boost::replace_all(fname, "[", "A_");
	boost::replace_all(fname, "]", "_");
	boost::replace_all(fname, ",", "_");
	return fname;
}

PlnType* PlnBlock::getFixedArrayType(PlnType* item_type, vector<int>& sizes, const string& mode)
{
	bool found_item = false;
	
	// Find item from Crrent block
	{
		auto t = std::find_if(types.begin(), types.end(),
				[item_type](PlnType* t) { return t->name == item_type->name; });

		if (t != types.end()) {
			found_item = true;
			BOOST_ASSERT(item_type == realType(*t, item_type->mode));
		}
	}
	
	if (!found_item) {
		if (PlnBlock* b = parentBlock(this)) {
			return b->getFixedArrayType(item_type, sizes, mode);

		} else { // toplevel
			vector<PlnType*> &basic_types = PlnType::getBasicTypes();	
			auto t = std::find_if(basic_types.begin(), basic_types.end(),
					[item_type](PlnType* t) { return t->name == item_type->name; });

			if (t != types.end()) {
				found_item = true;
				BOOST_ASSERT(item_type == realType(*t, item_type->mode));
			}

			BOOST_ASSERT(found_item);
		}
	}

	string name = PlnType::getFixedArrayName(item_type->getVarType(item_type->mode), sizes);
	for (auto t: types) 
		if (name == t->name) return t->getTypeWithMode(mode);
;
	
	auto t = new PlnFixedArrayType(name, item_type, sizes, this);
	t->mode = "wmo";
	t->default_mode = "wmo";
	t->rwo_type = t;
	types.push_back(t);
	return t->getTypeWithMode(mode);
}

PlnFunction* PlnBlock::getFunc(const string& func_name, vector<PlnValue*> &arg_vals, vector<PlnValue*> &out_arg_vals)
{
	PlnFunction* matched_func = NULL;
	vector<PlnFunction*> candidates;
	int amviguous_count = 0; 
	int is_perfect_match = false; 

	PlnBlock* b = this;
	do {
		for (auto f: b->funcs) {
			if (f->name == func_name) {
				candidates.push_back(f);
				if ((!f->has_va_arg)
						&& f->parameters.size() < (arg_vals.size()+out_arg_vals.size())) {
					goto next_func;
				}

				int i=0;
				int oi=0;
				bool do_cast = false;
				for (auto p: f->parameters) {
					bool is_output = p->iomode & PIO_OUTPUT;
					if (p->name == "...") {
						BOOST_ASSERT(i+oi+1 == f->parameters.size());
						if ((is_output && i != arg_vals.size())
								|| (!is_output && oi != out_arg_vals.size())) {
							goto next_func;
						}
						break;	// matched
					}

					PlnValue *arg_val;
					if (is_output) {
						if (oi >= out_arg_vals.size()) goto next_func;
						arg_val = out_arg_vals[oi];
						oi++;

					} else {
						if (i >= arg_vals.size() || !arg_vals[i]) {
							if (!p->dflt_value) goto next_func;
							// use default value
							i++;
							continue;
						}
						arg_val = arg_vals[i];
						i++;
					}

					PlnVarType* a_type = arg_val->getType();
					PlnTypeConvCap cap = p->var_type->canConvFrom(a_type);
					if (cap == TC_CANT_CONV) goto next_func;

					if (p->ptr_type == PTR_PARAM_MOVE && arg_val->asgn_type != ASGN_MOVE) {
						goto next_func;
					}
					if (p->ptr_type != PTR_PARAM_MOVE && arg_val->asgn_type == ASGN_MOVE) {
						goto next_func;
					}

					if (cap != TC_SAME) do_cast = true;
				}
				candidates.pop_back();

				if (is_perfect_match) {
					if (do_cast) goto next_func;
					else {// Existing another perfect match case is bug.
						// The case func f() && func f(int32 a = 0) exists and try call func();
						throw PlnCompileError(E_AmbiguousFuncCall, func_name);
					}

				} else {
					matched_func = f;
					if (do_cast) amviguous_count++;
					else is_perfect_match = true;
				}
			}
next_func:
			;
		}
	} while (b = parentBlock(b));
	
	if (is_perfect_match || amviguous_count == 1) return matched_func;

	if (!matched_func) {
		if (candidates.size()) {
			auto f = candidates[0];
			string pdef = boost::algorithm::join(f->getParamStrs(), ", ");
			string candidate_def = f->name + "(" + pdef + ")";
			throw PlnCompileError(E_NoMatchingFunction, func_name, candidate_def);
		}
		throw PlnCompileError(E_UndefinedFunction, func_name);
	}
	// if (amviguous_count >= 0)
	throw PlnCompileError(E_AmbiguousFuncCall, func_name);
}

PlnFunction* PlnBlock::getFuncProto(const string& func_name, vector<string>& param_types)
{
	PlnBlock* b = this;

	do {
		for (auto f: b->funcs) {
			if (f->name == func_name && f->parameters.size() == param_types.size()) {
				vector<string> f_ptypes = f->getParamStrs();
				BOOST_ASSERT(f_ptypes.size() == param_types.size());

				for (int i=0; i<param_types.size(); i++) {
					if (f_ptypes[i] != param_types[i])
						goto next;
				}

				BOOST_ASSERT(f->implement == NULL);
				return f;	// found complete match
			}
next:	;
		}
	} while (b = parentBlock(b));
	
	return NULL;
}

void PlnBlock::addFreeVars(vector<PlnExpression*> &free_vars, PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto v: variables) {
		if (parent_block && (v->ptr_type & PTR_OWNERSHIP)) {
			auto lt = si.get_lifetime(v);
			if (lt == VLT_ALLOCED || lt == VLT_INITED || lt == VLT_PARTLY_FREED) {
				PlnExpression* free_var = PlnFreer::getFreeEx(v);
				free_var->finish(da, si);
				free_vars.push_back(free_var);
			}
		}
	}
}

void PlnBlock::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	si.push_scope(this);
	for (auto s: statements) {
		s->finish(da, si);
		da.checkDataLeak();
	}
	
	addFreeVars(free_vars, da, si);
	
	for (auto v: variables)
		da.releaseDp(v->place);
	
	si.pop_owner_vars(this);
	si.pop_scope();
}

void PlnBlock::gen(PlnGenerator& g)
{
	g.comment("{");
	{
		// initalize all pointers.
		vector<unique_ptr<PlnGenEntity>> refs;
		for (auto v: variables) 
			if (v->ptr_type & PTR_REFERENCE) 
				refs.push_back(g.getEntity(v->place));

		g.genNullClear(refs);
	}

	for (auto s: statements) {
		s->gen(g);
		g.blank();
	}

	// TODO?: check condition: need not call this after jump statement.
	// Note: "return statement" frees vars insted of block when function end.
	for (auto free_var: free_vars) {
		free_var->gen(g);
	}

	g.comment("}");
}
