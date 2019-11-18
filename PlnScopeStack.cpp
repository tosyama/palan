/// Scope management class definition.
///
/// @file	PlnScopeStack.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <algorithm>

#include "PlnModel.h"
#include "models/PlnType.h"
#include "models/PlnVariable.h"
#include "PlnScopeStack.h"
#include "PlnConstants.h"

bool PlnScopeInfo::exists_current(PlnVariable* v)
{
	PlnScopeItem si = scope.back();
	auto ov = std::find_if(owner_vars.begin(), owner_vars.end(),
			[v, si](PlnScopeVarInfo &vi) { return vi.var == v && vi.scope == si; } );
	return ov != owner_vars.end();
}

void PlnScopeInfo::set_lifetime(PlnVariable* v, PlnVarLifetime lt)
{
	BOOST_ASSERT(v->var_type->mode[IDENTITY_MD] == 'm');
	auto ov = std::find_if(owner_vars.rbegin(), owner_vars.rend(),
			[v](PlnScopeVarInfo &vi) { return vi.var == v; } );

	BOOST_ASSERT(ov != owner_vars.rend());
	(*ov).lifetime = lt;
}

PlnVarLifetime PlnScopeInfo::get_lifetime(PlnVariable* v)
{
	BOOST_ASSERT(v->var_type->mode[IDENTITY_MD] == 'm');
	auto ov = std::find_if(owner_vars.rbegin(), owner_vars.rend(),
			[v](PlnScopeVarInfo &vi)
			{
				if (vi.var == v) return true;
				else return vi.var == v->container;
			} );


	if (ov == owner_vars.rend())
		return VLT_NOTEXIST;

	return (*ov).lifetime;
}

