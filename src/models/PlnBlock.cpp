/// Block model class definition.
///
/// Block model manage list of statements and local variables.
/// Block: "{" statements "}"
///
/// @file	PlnBlock.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>

#include "../PlnConstants.h"
#include "PlnType.h"
#include "PlnFunction.h"
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
#include "../PlnMessage.h"
#include "expressions/PlnFunctionCall.h"
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
	for (auto t: typeinfos)
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

static bool existsVar(PlnBlock* block, const string& var_name, bool do_check_ancestor_blocks)
{
	for (auto v: block->variables)
		if (v->name == var_name) return true;
	for (auto c: block->consts)
		if (c.name == var_name) return true;
	for (auto g: block->globals)
		if (g->name == var_name) return true;
	
	if (block->parent_func) {
		for (auto&rv: block->parent_func->return_vals)
			if (rv.local_var->name == var_name) return true;
		for (auto p: block->parent_func->parameters)
			if (p->var->name == var_name) return true;

	}

	if (do_check_ancestor_blocks) {
		PlnBlock *b = block->parent_block;
		while (b) {
			for (auto v: b->variables)
				if (v->name == var_name) return true;
			for (auto c: b->consts)
				if (c.name == var_name) return true;
			b = b->parent_block;
		}
	}

	if (do_check_ancestor_blocks) {
		if (block->getGlobalVariable(var_name))
			return true;
	}

	return false;
}

PlnVariable* PlnBlock::declareVariable(const string& var_name, PlnVarType* var_type, bool do_check_ancestor_blocks)
{
	if (existsVar(this, var_name, do_check_ancestor_blocks))
		return NULL;

	PlnVariable* v = new PlnVariable();
	v->name = var_name;
	v->var_type = var_type ? var_type : variables.back()->var_type;

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
		
		for (auto v: b->globals)
			if (v->name == var_name)
				return v;

		if (b->parent_block)
			b = b->parent_block;

		else if (b->parent_func) {
			for (auto& rv: parent_func->return_vals)
				if (rv.local_var->name == var_name) return rv.local_var;
			for (auto p: parent_func->parameters)
				if (p->var->name == var_name) return p->var;

			return this->parent_func->parent->getGlobalVariable(var_name);

		} else
			return NULL;;
	}

	BOOST_ASSERT(false);
}

static PlnBlock* parentBlock(PlnBlock* block)
{
	if (block->parent_block) return block->parent_block;
	if (block->parent_func) return block->parent_func->parent;
	return NULL;
}

PlnVariable* PlnBlock::declareGlobalVariable(const string& var_name, PlnVarType* var_type, bool is_extern)
{
	BOOST_ASSERT(is_extern == true);	// false is not implemented yet.

	if (existsVar(this, var_name, false))
		return NULL;
	
	PlnVariable* v = new PlnVariable();
	v->name = var_name;
	v->var_type = var_type;
	v->is_global = true;

	globals.push_back(v);
	return v;
}

PlnVariable* PlnBlock::getGlobalVariable(const string& var_name)
{
	PlnBlock* b = this;
	do {
		auto global_var = find_if(b->globals.begin(), b->globals.end(),
			[var_name](PlnVariable* v) { return v->name == var_name; } );

		if (global_var != b->globals.end()) {
			return *global_var;
		}

	} while (b = parentBlock(b));

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
	PlnTypeInfo* t = new PlnTypeInfo();
	t->name = type_name;
	t->default_mode = "wmr";
	t->data_type = DT_OBJECT;
	t->data_size = 0;
	typeinfos.push_back(t);
}

void PlnBlock::declareType(const string& type_name, vector<PlnStructMemberDef*> &members)
{
	auto t = new PlnStructTypeInfo(type_name, members, this, "wmh");
	typeinfos.push_back(t);
}

void PlnBlock::declareAliasType(const string& type_name, PlnVarType* orig_type)
{
	BOOST_ASSERT(orig_type);

	auto t = new PlnAliasTypeInfo(type_name, orig_type, orig_type->typeinf);
	typeinfos.push_back(t);
}

static PlnVarType* realType(PlnTypeInfo *t, const string& mode) {
	if (t->type == TP_ALIAS) {
		PlnAliasTypeInfo *at = static_cast<PlnAliasTypeInfo*>(t);
		PlnVarType *vtype = at->orig_type->getVarType(mode);
		BOOST_ASSERT(vtype->typeinf->type != TP_ALIAS);
		return vtype;
	}
	return t->getVarType(mode);
}

PlnVarType* PlnBlock::getType(const string& type_name, const string& mode)
{
	// Crrent blockstatic_cast<PlnAliasType*>(t)->orig_type
	{
		auto t = std::find_if(typeinfos.begin(), typeinfos.end(),
				[type_name](PlnTypeInfo* t) { return t->name == type_name; });

		if (t != typeinfos.end())
			return realType(*t, mode);
	}

	// Parent block
	if (PlnBlock* b = parentBlock(this)) {
		return b->getType(type_name, mode);
	}

	// Search default type if toplevel: parentBlock(this) == NULL
	{
		vector<PlnTypeInfo*> &basic_types = PlnTypeInfo::getBasicTypes();

		auto t = std::find_if(basic_types.begin(), basic_types.end(),
				[type_name](PlnTypeInfo* t) { return t->name == type_name; });

		if (t != basic_types.end())
			return realType(*t, mode);
	}

	return NULL;
}

string PlnBlock::generateFuncName(string fname, vector<PlnTypeInfo*> ret_types, vector<PlnTypeInfo*> arg_types)
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
	boost::replace_all(fname, "@", "R_");
	boost::replace_all(fname, "#", "S_");
	return fname;
}

PlnVarType* PlnBlock::getFixedArrayType(PlnVarType* item_type, vector<int>& sizes, const string& mode)
{
	PlnBlock *defined_block = this;
	bool found_item = false;
	
	// Find item from Crrent block
	{
		auto t = std::find_if(typeinfos.begin(), typeinfos.end(),
				[item_type](PlnTypeInfo* t) { return t == item_type->typeinf; });

		if (t != typeinfos.end()) {
			defined_block = this;
			found_item = true;
		}
	}
	
	if (!found_item) {
		if (PlnBlock* b = parentBlock(this)) {
			return b->getFixedArrayType(item_type, sizes, mode);

		} else { // toplevel
			vector<PlnTypeInfo*> &basic_types = PlnTypeInfo::getBasicTypes();	
			auto t = std::find_if(basic_types.begin(), basic_types.end(),
					[item_type](PlnTypeInfo* t) { return t == item_type->typeinf; });

			if (t != typeinfos.end()) {
				defined_block = parent_module->toplevel;
				found_item = true;
				BOOST_ASSERT(item_type == realType(*t, item_type->mode));
			}

			BOOST_ASSERT(found_item);
		}
	}

	string name = PlnTypeInfo::getFixedArrayName(item_type, sizes);
	for (auto t: defined_block->typeinfos) 
		if (name == t->name) {
			PlnFixedArrayVarType *vtype = static_cast<PlnFixedArrayVarType*>(t->getVarType(mode));
			vtype->sizes = sizes;
			return vtype;
		}
	
	auto t = new PlnFixedArrayTypeInfo(name, item_type, sizes, this);
	t->default_mode = "wmh";
	defined_block->typeinfos.push_back(t);
	PlnFixedArrayVarType *vtype = static_cast<PlnFixedArrayVarType*>(t->getVarType(mode));
	vtype->sizes = sizes;
	return vtype;
}

PlnBlock* PlnBlock::getTypeDefinedBlock(PlnVarType* var_type)
{
	PlnTypeInfo *typeinfo = var_type->typeinf;

	// Find item from Crrent block
	{
		auto t = std::find_if(typeinfos.begin(), typeinfos.end(),
				[typeinfo](PlnTypeInfo* t) { return t == typeinfo; });

		if (t != typeinfos.end()) {
			return this;
		}
	}

	if (PlnBlock* b = parentBlock(this)) {
		return b->getTypeDefinedBlock(var_type);

	} else { // toplevel always return NULL;
		vector<PlnTypeInfo*> &basic_types = PlnTypeInfo::getBasicTypes();	
		auto t = std::find_if(basic_types.begin(), basic_types.end(),
				[typeinfo](PlnTypeInfo* t) { return t == typeinfo; });

		BOOST_ASSERT(t != basic_types.end());
		return parent_module->toplevel;
	}
	
}

PlnFunction* PlnBlock::getFunc(const string& func_name, vector<PlnArgInf> &arg_infs)
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
				if ((!f->has_va_arg) && f->parameters.size() < arg_infs.size()) {
					goto next_func;
				}

				int ii=-1, oi=-1;
				bool do_cast = false;

				for (auto p: f->parameters) {
					bool is_input = p->iomode == PIO_INPUT;
					int ai;
					if (is_input) {
						// search next input
						ii++;
						for (; ii<arg_infs.size(); ++ii) {
							if (arg_infs[ii].iomode == PIO_INPUT)
								break;
						}
						ai = ii;

					} else {
						// search next output
						oi++;
						for (; oi<arg_infs.size(); ++oi) {
							if (arg_infs[oi].iomode == PIO_OUTPUT)
								break;
						}
						ai = oi;
					}

					if (p->var->name == "...") {
						for (int i = (is_input ? oi : ii)+1; i<arg_infs.size();++i)
							if (arg_infs[i].iomode != p->iomode)
								goto next_func;

						break;	// Matched
					}

					if (ai >= arg_infs.size() || !arg_infs[ai].var_type) {
						if (!p->dflt_value) goto next_func;
						else continue;	// variable argument or use default value
					}

					PlnArgInf& ainf = arg_infs[ai];

					// Check conpatibilty of type.
					PlnAsgnType atype;
					switch (p->passby) {
						case FPM_IN_BYVAL:
							atype = ASGN_COPY; break;
						case FPM_IN_BYREF:
							atype = ASGN_COPY_REF; break;
						case FPM_IN_BYREF_CLONE:
							atype = ASGN_COPY; break;
						case FPM_IN_BYREF_MOVEOWNER:
							atype = ASGN_MOVE; break;
						case FPM_OUT_BYREF:
							atype = ASGN_COPY_REF; break;
						case FPM_OUT_BYREFADDR:
							atype = ASGN_COPY_REF; break;
						case FPM_OUT_BYREFADDR_GETOWNER:
							atype = ASGN_MOVE; break;

						default:
							BOOST_ASSERT(false);
					}
					PlnTypeConvCap cap = p->var->var_type->canCopyFrom(ainf.var_type, atype);
					if (cap == TC_CANT_CONV) goto next_func;

					bool is_move = p->passby == FPM_IN_BYREF_MOVEOWNER || p->passby == FPM_OUT_BYREFADDR_GETOWNER;

					if (is_move && ainf.opt != AG_MOVE) {
						goto next_func;
					}
					if (!is_move && ainf.opt == AG_MOVE) {
						goto next_func;
					}

					if (cap != TC_SAME) do_cast = true;
				}

				// Matched
				candidates.pop_back();

				if (is_perfect_match) {
					if (do_cast) goto next_func;
					else {// Existing another perfect match case is bug.
						// The case func f() && func f(int31 a = 0) exists and try call func();
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
	if (parent_block) {
		for (auto v: variables) {
			if (v->var_type->mode[ALLOC_MD]=='h') {
				auto lt = si.get_lifetime(v);
				if (lt == VLT_ALLOCED || lt == VLT_INITED || lt == VLT_PARTLY_FREED) {
					PlnExpression* free_var = v->getFreeEx();
					free_var->finish(da, si);
					free_vars.push_back(free_var);
				}
			} else if (v->var_type->data_type() == DT_OBJECT) {
				PlnExpression* free_var = v->getInternalFreeEx();
				if (free_var) {
					free_var->finish(da, si);
					free_vars.push_back(free_var);
				}
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
	
	for (auto v: variables) {
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
		for (auto v: variables) {
			char alloc_mode = v->var_type->mode[ALLOC_MD];
			if (alloc_mode == 'h' || alloc_mode == 'r')
				refs.push_back(g.getEntity(v->place));
		}

		g.genNullClear(refs);
	}

	for (auto s: statements) {
		s->gen(g);
		g.blank();
	}

	// TODO?: check condition: need not call this after jump statement.
	// Note: "return statement" frees vars insted of block when function end.
	// Note: Generator will delete unreachable block. So, this ToDo is low priority.
	for (auto free_var: free_vars) {
		free_var->gen(g);
	}

	g.comment("}");
}
