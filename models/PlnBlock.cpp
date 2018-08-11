/// Block model class definition.
///
/// Block model manage list of statements and local variables.
/// Block: "{" statements "}"
///
/// @file	PlnBlock.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "PlnArray.h"
#include "PlnExpression.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"

using std::endl;
using boost::format;
using boost::adaptors::reverse;

PlnBlock::PlnBlock()
	: parent_func(NULL), parent_block(NULL)
{
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

bool PlnBlock::declareConst(const string& name, PlnExpression* exp)
{
	for (auto v: variables)
		if (v->name == name) return false;
	for (auto c: consts)
		if (c.name == name) return false;

	consts.push_back( {name, exp} );
	return true;
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
			if (lt == VLT_ALLOCED || lt == VLT_INITED) {
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
