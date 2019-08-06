/// Structure type class definition.
///
/// @file	PlnStructType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnModel.h"
#include "../../PlnConstants.h"
#include "../PlnGeneralObject.h"
#include "PlnStructType.h"

PlnStructMember::PlnStructMember(PlnType *type, string name)
	: type(type), name(name), offset(0)
{
}

PlnStructType::PlnStructType(const string &name, vector<PlnStructMember*> &members)
	: PlnType(TP_STRUCT), members(move(members))
{
	this->name = name;
	data_type = DT_OBJECT_REF;
	size = 8;
	int alloc_size = 0;
	int max_member_size = 1;
	bool has_object_member = false;

	for (auto m: this->members) {
		int member_size = m->type->size;
		// padding
		if (alloc_size % member_size) {
			alloc_size = (alloc_size / member_size+1) * member_size;
		}

		if (member_size > max_member_size) {
			max_member_size = member_size;
		}
		alloc_size += m->type->size;

		if (m->type->data_type == DT_OBJECT_REF) {
			has_object_member = true;
		}
	}

	// last padding
	if (alloc_size % max_member_size) {
		alloc_size = (alloc_size / max_member_size+1) * max_member_size;
	}

	inf.obj.is_fixed_size = true;
	inf.obj.alloc_size = alloc_size;

	if (has_object_member)
		BOOST_ASSERT(false);
	else {
		allocator = new PlnSingleObjectAllocator(alloc_size);
		freer = new PlnSingleObjectFreer();
	}
}

PlnTypeConvCap PlnStructType::canConvFrom(PlnType *src) {
	if (this == src)
		return TC_SAME;
	
	return TC_CANT_CONV;
}
