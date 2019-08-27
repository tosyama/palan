/// Struct member model class definition.
///
/// PlnStructMember returns member of sturct
/// that indicated by name.
/// e.g.) s.i 
///
/// @file	PlnStructMember.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

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

PlnStructMember::PlnStructMember(PlnExpression* sturct_ex, string member_name)
	: PlnExpression(ET_STRUCTMEMBER), struct_ex(sturct_ex), def(NULL)
{
	BOOST_ASSERT(struct_ex->values.size() == 1);
	PlnType *t = struct_ex->values[0].getType();
	if (t->type == TP_STRUCT) {
		PlnStructType *st = static_cast<PlnStructType*>(t);
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

	} else
		BOOST_ASSERT(false);

	BOOST_ASSERT(struct_ex->values[0].type == VL_VAR);
	auto var = new PlnVariable();
	auto struct_var = struct_ex->values[0].inf.var;
	var->name = struct_var->name + "." + def->name;
	var->var_type = def->type;
	if (struct_var->container)
		var->container = struct_var->container;
	else
		var->container = struct_var;
	
	if (def->type->data_type == DT_OBJECT_REF) {
		var->ptr_type = PTR_REFERENCE | PTR_INDIRECT_ACCESS;
		var->ptr_type |= var->container->ptr_type & (PTR_OWNERSHIP | PTR_READONLY);

	} else {
		var->ptr_type = NO_PTR | PTR_INDIRECT_ACCESS;
		var->ptr_type |= var->container->ptr_type & PTR_READONLY;
	}
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
		member_var->place = new PlnDataPlace(member_var->var_type->size, member_var->var_type->data_type);
		member_var->place->comment = &member_var->name;
	}

	auto base_dp = da.prepareObjBasePtr();
	struct_ex->data_places.push_back(base_dp);
	struct_ex->finish(da, si);

	auto member_dp = member_var->place;
	da.setIndirectObjDp(member_dp, base_dp, NULL, def->offset);
	
	if (data_places.size()) {
		da.pushSrc(data_places[0], member_dp);
	}
}

void PlnStructMember::gen(PlnGenerator& g)
{
	// for lval & rval
	struct_ex->gen(g);

	// rval
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

