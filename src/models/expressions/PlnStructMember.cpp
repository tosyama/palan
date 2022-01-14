/// Struct member model class definition.
///
/// PlnStructMember returns member of sturct
/// that indicated by name.
/// e.g.) s.i 
///
/// @file	PlnStructMember.cpp
/// @copyright	2019-2020 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../PlnType.h"
#include "../types/PlnStructType.h"
#include "../PlnVariable.h"
#include "PlnStructMember.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../types/PlnFixedArrayType.h"

PlnStructMember::PlnStructMember(PlnExpression* sturct_ex, string member_name)
	: PlnExpression(ET_STRUCTMEMBER), struct_ex(sturct_ex), def(NULL)
{
	BOOST_ASSERT(struct_ex->values.size() == 1);
	PlnTypeInfo *t = struct_ex->values[0].getVarType()->typeinf;
	if (t->type == TP_STRUCT) {
		PlnStructTypeInfo *st = static_cast<PlnStructTypeInfo*>(t);
		for (auto md: st->members) {
			if (md->name == member_name) {
				def = md;
				break;
			}
		}

		if (!def) {
			PlnCompileError err(E_NoMemberName, t->name, member_name);
			throw err;
		}

	} else {
		PlnCompileError err(E_NoMemberName, t->name, member_name);
		throw err;
	}

	BOOST_ASSERT(struct_ex->values[0].type == VL_VAR);
	auto var = new PlnVariable();
	auto struct_var = struct_ex->values[0].inf.var;
	var->name = struct_var->name + "." + def->name;
	if (struct_var->container)
		var->container = struct_var->container;
	else
		var->container = struct_var;
	
	string mode = def->type->mode;
	if (var->container->var_type->mode[ACCESS_MD] == 'r') {
		mode[ACCESS_MD] = 'r';
	}
	var->var_type = def->type->typeinf->getVarType(mode);
	if (def->type->typeinf->type == TP_FIXED_ARRAY) {
		static_cast<PlnFixedArrayVarType*>(var->var_type)->sizes = static_cast<PlnFixedArrayVarType*>(def->type)->sizes;
	}
	var->is_indirect = true;
	var->is_tmpvar = var->container->is_tmpvar;

	values.push_back(var);
}

PlnStructMember::~PlnStructMember()
{
	delete struct_ex;
}

void PlnStructMember::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto member_var = values[0].inf.var;

	// PlnValue::getDataPlace may alloc dp.
	if (!member_var->place) {
		member_var->place = new PlnDataPlace(member_var->var_type->size(), member_var->var_type->data_type());
		member_var->place->comment = &member_var->name;
	}

	auto base_dp = da.prepareObjBasePtr();
	struct_ex->data_places.push_back(base_dp);
	struct_ex->finish(da, si);

	auto member_dp = member_var->place;
	da.setIndirectObjDp(member_dp, base_dp, NULL, def->offset);
	da.popSrc(base_dp);
	
	if (data_places.size()) {
		if (member_dp->data_type == DT_OBJECT) {
			data_places[0]->load_address = true;
		}
		da.pushSrc(data_places[0], member_dp);
	}
}

void PlnStructMember::gen(PlnGenerator& g)
{
	// for lval & rval
	struct_ex->gen(g);

	auto member_var = values[0].inf.var;
	auto member_dp = member_var->place;
	auto base_dp = member_dp->data.indirect.base_dp;

	g.genLoadDp(base_dp, false);
	g.genSaveDp(base_dp);

	// rval
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

vector<PlnExpression*> PlnStructMember::getAllStructMembers(PlnVariable* var)
{
	BOOST_ASSERT(var->var_type->typeinf->type == TP_STRUCT);
	vector<PlnExpression*> member_exs;

	PlnStructTypeInfo *stype = static_cast<PlnStructTypeInfo*>(var->var_type->typeinf);
	for (auto member: stype->members) {
		PlnExpression* var_ex = new PlnExpression(var);
		PlnExpression* member_ex = new PlnStructMember(var_ex, member->name);
		member_exs.push_back(member_ex);
	}

	return member_exs;
}
