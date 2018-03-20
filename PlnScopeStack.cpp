#include "PlnModel.h"
#include "models/PlnVariable.h"
#include "PlnScopeStack.h"
#include <boost/assert.hpp>

bool PlnScopeInfo::exists_current(PlnVariable* v)
{
	PlnScopeItem si = scope.back();
	auto ov = std::find_if(owner_vars.begin(), owner_vars.end(),
			[v, si](PlnScopeVarInfo &vi) { return vi.var == v && vi.scope == si; } );
	return ov != owner_vars.end();
}

void PlnScopeInfo::set_lifetime(PlnVariable* v, PlnVarLifetime lt)
{
	BOOST_ASSERT(v->ptr_type & PTR_OWNERSHIP);
	auto ov = std::find_if(owner_vars.begin(), owner_vars.end(),
			[v](PlnScopeVarInfo &vi) { return vi.var == v; } );

	BOOST_ASSERT(ov != owner_vars.end());
	(*ov).lifetime = lt;
}

PlnVarLifetime PlnScopeInfo::get_lifetime(PlnVariable* v)
{
	BOOST_ASSERT(v->ptr_type & PTR_OWNERSHIP);
	auto ov = std::find_if(owner_vars.begin(), owner_vars.end(),
			[v](PlnScopeVarInfo &vi)
			{
				if (vi.var == v) return true;
				else return vi.var == v->container;
			} );

	BOOST_ASSERT(ov != owner_vars.end());
	return (*ov).lifetime;
}
