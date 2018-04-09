/// Assignment Item class definition.
///
/// Imprementation of all assignment item.
///
/// @file	PlnAssignItem.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "../PlnAssignment.h"
#include "../../PlnVariable.h"
#include "../../PlnType.h"
#include "../../../PlnDataAllocator.h"
#include "../../../PlnGenerator.h"
#include "../../../PlnConstants.h"
#include "../../../PlnScopeStack.h"
#include "../../PlnHeapAllocator.h"
#include "../PlnDivOperation.h"
#include "PlnAssignItem.h"

// PlnDstItem
class PlnDstItem {
public:
	virtual bool ready() { return false; }	// Temporary.

	virtual PlnAsgnType getAssginType() { return NO_ASGN; };
	virtual PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) { return NULL; }
	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si) { BOOST_ASSERT(false); }
	virtual void gen(PlnGenerator& g) { }

	static PlnDstItem* createDstItem(PlnExpression* ex);
};

// implementations 
#include "PlnAssignPrimitiveItem.h"
#include "PlnAssignWorkValsItem.h"
#include "PlnAssignObjectRefItem.h"

PlnAssignItem* PlnAssignItem::createAssignItem(PlnExpression* ex)
{
	if (ex->type == ET_VALUE) {
		PlnValue v = ex->values[0];
		if (v.type == VL_LIT_INT8 || v.type == VL_LIT_UINT8 || v.type == VL_RO_DATA) {
			return new PlnAssignPrimitiveItem(ex);
		}
		if (v.type == VL_VAR) {
			int dt = ex->values[0].getType()->data_type;
			if (dt == DT_SINT || dt == DT_UINT) {
				return new PlnAssignPrimitiveItem(ex);

			} else if (dt == DT_OBJECT_REF) {
				return new PlnAssignObjectRefItem(ex);
			}
		}
	}

	if (ex->type == ET_ADD || ex->type == ET_MUL || ex->type == ET_NEG) {
		BOOST_ASSERT(ex->values.size() == 1);
		PlnValue v = ex->values[0];
		BOOST_ASSERT(v.type == VL_WORK);
		PlnType* t = ex->values[0].inf.wk_type;
		BOOST_ASSERT(t->data_type != DT_OBJECT_REF);

		return new PlnAssignPrimitiveItem(ex);
	}

	if (ex->type == ET_DIV) {
		auto dex = static_cast<PlnDivOperation*>(ex);
		if (dex->div_type == DV_MOD)
			return new PlnAssignPrimitiveItem(ex);
		else
			return new PlnAssignWorkValsItem(ex);
	}

	if (ex->type == ET_FUNCCALL) {
		return new PlnAssignWorkValsItem(ex);
	}

	return new PlnAssignItem();
}


#include "PlnDstPrimitiveItem.h"
#include "PlnDstCopyObjectItem.h"
#include "PlnDstMoveObjectItem.h"

PlnDstItem* PlnDstItem::createDstItem(PlnExpression* ex)
{
	if (ex->type == ET_VALUE) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		PlnType* t = ex->values[0].getType();
		if (t->data_type == DT_OBJECT_REF) {
			auto asgn_type = ex->values[0].asgn_type;
			if (asgn_type == ASGN_COPY) {
				return new PlnDstCopyObjectItem(ex);
			} else if (asgn_type == ASGN_MOVE) {
				return new PlnDstMoveObjectItem(ex);
			} else if (asgn_type == ASGN_COPY_REF) {
				return new PlnDstPrimitiveItem(ex);
			}
		} else {
			return new PlnDstPrimitiveItem(ex);
		}
	} 

	if (ex->type == ET_ARRAYITEM) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		int dt = ex->values[0].getType()->data_type;
		if (dt == DT_SINT || dt == DT_UINT) {
			return new PlnDstPrimitiveItem(ex);
		}
	}

	return new PlnDstItem();
}

