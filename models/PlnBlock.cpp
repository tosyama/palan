/// Block model class definition.
///
/// Block model manage list of statements and local variables.
/// Block: "{" statements "}"
///
/// @file	PlnBlock.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <algorithm>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "PlnExpression.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "../PlnMessage.h"
#include "../PlnException.h"

PlnBlock::PlnBlock()
	: parent_func(NULL), parent_block(NULL)
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
}

void PlnBlock::setParent(PlnFunction* f)
{
	parent_func = f;
	parent_block = NULL;
}

void PlnBlock::setParent(PlnBlock* b)
{
	parent_func = b->parent_func;
	parent_block = b;
}

PlnVariable* PlnBlock::declareVariable(const string& var_name, vector<PlnType*>& var_type, bool is_owner)
{
	for (auto v: variables)
		if (v->name == var_name) return NULL;
	for (auto c: consts)
		if (c.name == var_name) return NULL;

	PlnVariable* v = new PlnVariable();
	v->name = var_name;
	v->container = NULL;

	if (var_type.size() > 0) {
		v->var_type = move(var_type);
		if (v->var_type.back()->data_type == DT_OBJECT_REF) {
			v->ptr_type = is_owner ? PTR_REFERENCE | PTR_OWNERSHIP
							: PTR_REFERENCE;
		} else v->ptr_type = NO_PTR;
	} else {
		v->var_type = variables.back()->var_type;
		v->ptr_type = variables.back()->ptr_type;
	}

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

PlnFunction* PlnBlock::getFunc(const string& func_name, vector<PlnValue*>& arg_vals)
{
	PlnFunction* matched_func = NULL;
	int amviguous_count = 0; 
	int is_perfect_match = false; 

	PlnBlock* b = this;
	do {
		for (auto f: b->funcs) {
			if (f->name == func_name) {
				if (f->parameters.size() < arg_vals.size()) {
					// Excluded C and System call for now
					if (f->type == FT_C || f->type == FT_SYS) return f;
					goto next_func;
				}

				int i=0;
				bool do_cast = false;
				for (auto p: f->parameters) {
					if (i+1>arg_vals.size() || !arg_vals[i]) {
						if (!p->dflt_value) goto next_func;
					} else {
						PlnType* a_type = arg_vals[i]->getType();
						PlnTypeConvCap cap = p->var_type.back()->canConvFrom(a_type);
						if (cap == TC_CANT_CONV) goto next_func;

						if (p->ptr_type == PTR_PARAM_MOVE && arg_vals[i]->asgn_type != ASGN_MOVE) {
							goto next_func;
						}
						if (p->ptr_type != PTR_PARAM_MOVE && arg_vals[i]->asgn_type == ASGN_MOVE) {
							goto next_func;
						}

						if (cap != TC_SAME) do_cast = true;
					}
					++i;
				}

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

	if (!matched_func) throw PlnCompileError(E_UndefinedFunction, func_name);
	// if (amviguous_count >= 0)
	throw PlnCompileError(E_AmbiguousFuncCall, func_name);
}

PlnFunction* PlnBlock::getFuncProto(const string& func_name, vector<string>& param_types)
{
	PlnBlock* b = this;

	do {
		for (auto f: b->funcs) {
			if (f->name == func_name && f->parameters.size() == param_types.size()) {

				for (int i=0; i<param_types.size(); i++) {
					string pt_name = f->parameters[i]->var_type.back()->name;
					if (f->parameters[i]->ptr_type == PTR_PARAM_MOVE) {
						pt_name += ">>";
					}
					if (pt_name != param_types[i]) {
						goto next;
					}
				}

				BOOST_ASSERT(f->implement == NULL);
				return f;
			}
next:	;
		}
	} while (b = parentBlock(b));
	
	return NULL;
}

void PlnBlock::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	si.push_scope(this);
	for (auto s: statements) {
		s->finish(da, si);
		da.checkDataLeak();
	}
	
	for (auto v: variables) {
		if (parent_block && (v->ptr_type & PTR_OWNERSHIP)) {
			auto lt = si.get_lifetime(v);
			if (lt == VLT_ALLOCED || lt == VLT_INITED || lt == VLT_PARTLY_FREED) {
				PlnExpression* free_var = PlnFreer::getFreeEx(v);
				free_var->finish(da, si);
				free_vars.push_back(free_var);
			}
		}
		da.releaseDp(v->place);
	}
	
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
