/// Reference value model class definition.
///
/// PlnReferenceValue returns a value that indicated by reference.
///
/// @file	PlnReferenceValue.cpp
/// @copyright	2020 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "../../PlnConstants.h"
#include "PlnReferenceValue.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../PlnVariable.h"
#include "../PlnType.h"

PlnReferenceValue::PlnReferenceValue(PlnExpression *refvar_ex)
	: PlnExpression(ET_REFVALUE), refvar_ex(refvar_ex)
{
	BOOST_ASSERT(refvar_ex->values[0].type == VL_VAR);

	auto var = new PlnVariable();
	auto ref_var = refvar_ex->values[0].inf.var;
	auto ref_vartype = ref_var->var_type;
	BOOST_ASSERT(ref_vartype->mode[ALLOC_MD] == 'r');
	BOOST_ASSERT(ref_vartype->typeinf->type == TP_PRIMITIVE);

	var->name = ref_var->name + "@";
	string mode = "---";
	mode[ACCESS_MD] = ref_vartype->mode[ACCESS_MD];
	var->var_type = ref_vartype->typeinf->getVarType(mode);	// get default type

	BOOST_ASSERT(ref_var->container);
	var->container = ref_var->container;

	var->is_indirect = true;
	var->is_tmpvar = var->container->is_tmpvar;
	var->place = NULL;

	values.push_back(var);
}

PlnReferenceValue::~PlnReferenceValue()
{
	delete refvar_ex;
}

PlnExpression* PlnReferenceValue::adjustTypes(const vector<PlnVarType*> &types)
{
	BOOST_ASSERT(types.size() == 1);
	PlnVarType *vtype = values[0].getVarType();
	if (types[0]->canCopyFrom(vtype, ASGN_COPY) == TC_CANT_CONV) {
		PlnCompileError err(E_IncompatibleTypeAssign, vtype->name(), types[0]->name());
		err.loc = loc;
		throw err;
	}
	
	if (types[0]->mode[ALLOC_MD] == 'r') { // lea (%src)  %dst -> mov %src %dst
		auto ret_ex = refvar_ex;
		refvar_ex = NULL;
		delete this;
		return ret_ex;
	}

	return this;
}

void PlnReferenceValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto base_dp = da.prepareObjBasePtr();
	refvar_ex->data_places.push_back(base_dp);
	refvar_ex->finish(da, si);

	auto ref_var = values[0].inf.var;
	if (!ref_var->place) {
		ref_var->place = new PlnDataPlace(ref_var->var_type->size(), ref_var->var_type->data_type());
		ref_var->place->comment = &ref_var->name;
	}

	da.setIndirectObjDp(ref_var->place, base_dp, NULL, 0);

	da.popSrc(base_dp);
	if (data_places.size()) {
		da.pushSrc(data_places[0], ref_var->place);
	}
}

void PlnReferenceValue::gen(PlnGenerator& g)
{
	auto base_dp = refvar_ex->data_places[0];
	refvar_ex->gen(g);
	g.genLoadDp(base_dp, false);
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}
