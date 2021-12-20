/// Assignment Item class definition.
///
/// Imprementation of all assignment item.
///
/// @file	PlnAssignItem.cpp
/// @copyright	2018-2021 YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "../../../PlnConstants.h"
#include "../PlnAssignment.h"
#include "../../PlnVariable.h"
#include "../../PlnType.h"
#include "../../../PlnDataAllocator.h"
#include "../../../PlnGenerator.h"
#include "../../../PlnScopeStack.h"
#include "../../../PlnMessage.h"
#include "../../../PlnException.h"
#include "../PlnDivOperation.h"
#include "../PlnMemCopy.h"
#include "../PlnClone.h"
#include "../PlnArrayValue.h"
#include "../PlnArrayItem.h"
#include "../PlnStructMember.h"
#include "../../types/PlnFixedArrayType.h"
#include "PlnAssignItem.h"

// PlnDstItem
class PlnDstItem {
public:
	PlnDataPlace* place;
	bool need_save;
	PlnDstItem(): place(NULL), need_save(false) {}
	virtual ~PlnDstItem() {}

	virtual PlnAsgnType getAssginType() = 0;
	virtual void setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) = 0;
	// LCOV_EXCL_START
	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si) {
		BOOST_ASSERT(false);
	}
	// LCOV_EXCL_STOP
	virtual void gen(PlnGenerator& g) = 0;

	static PlnDstItem* createDstItem(PlnExpression* ex, bool need_save);
};

// implementations 
#include "PlnAssignPrimitiveItem.h"
#include "PlnAssignWorkValsItem.h"
#include "PlnAssignObjectRefItem.h"
#include "PlnAssignIndirectObjItem.h"
#include "PlnChainAssignItem.h"
#include "PlnAssignArrayValue.h"

PlnAssignItem* PlnAssignItem::createAssignItem(PlnExpression* ex)
{
	if (ex->type == ET_VALUE) {
		PlnValue &v = ex->values[0];
		if (v.type == VL_LIT_INT8 || v.type == VL_LIT_UINT8 || v.type == VL_LIT_FLO8) {
			return new PlnAssignPrimitiveItem(ex);
		} else if (v.type == VL_LIT_STR) {
			return new PlnAssignObjectRefItem(ex);
		}

		if (v.type == VL_LIT_ARRAY) {
			return new PlnAssignArrayValue(v.inf.arrValue);
			// return new PlnAssignObjectRefItem(ex);
		}

		if (v.type == VL_VAR) {
			int dt = ex->values[0].getVarType()->data_type();
			if (dt == DT_SINT || dt == DT_UINT || dt == DT_FLOAT) {
				return new PlnAssignPrimitiveItem(ex);

			} else if (dt == DT_OBJECT_REF) {
				return new PlnAssignObjectRefItem(ex);

			} else {
				BOOST_ASSERT(dt == DT_OBJECT);
				return new PlnAssignObjectRefItem(ex);
				
			}
		}
	}

	if (ex->type == ET_ADD || ex->type == ET_MUL || ex->type == ET_NEG
			|| ex->type == ET_AND || ex->type == ET_OR || ex->type == ET_CMP) {
		BOOST_ASSERT(ex->values.size() == 1);
		PlnValue v = ex->values[0];
		BOOST_ASSERT(v.type == VL_WORK);
		PlnVarType* t = ex->values[0].inf.wk_type;
		BOOST_ASSERT(t->data_type() != DT_OBJECT_REF);

		return new PlnAssignPrimitiveItem(ex);
	}

	if (ex->type == ET_DIV) {
		auto dex = static_cast<PlnDivOperation*>(ex);
		if (dex->div_type == DV_MOD)
			return new PlnAssignPrimitiveItem(ex);
		else
			return new PlnAssignWorkValsItem(ex);
	}

	if (ex->type == ET_FUNCCALL || ex->type == ET_CHAINCALL) {
		return new PlnAssignWorkValsItem(ex);
	}

	if (ex->type == ET_ARRAYITEM || ex->type == ET_STRUCTMEMBER || ex->type == ET_REFVALUE) {
		BOOST_ASSERT(ex->values.size() == 1);
		int dt = ex->values[0].getVarType()->data_type();
		if (dt == DT_SINT || dt == DT_UINT || dt == DT_FLOAT) {
			return new PlnAssignPrimitiveItem(ex);
		} else if (dt == DT_OBJECT_REF || dt == DT_OBJECT) {
			return new PlnAssignIndirectObjItem(ex);
		}
	}

	if (ex->type == ET_ASSIGN) {
		return new PlnChainAssignItem(ex);
	}

	if (ex->type == ET_ARRAYVALUE) {
		return new PlnAssignArrayValue(ex);
	}

	BOOST_ASSERT(false);
}	// LCOV_EXCL_LINE

#include "PlnDstPrimitiveItem.h"
#include "PlnDstCopyObjectItem.h"
#include "PlnDstMoveObjectItem.h"
#include "PlnDstMoveIndirectObjItem.h"

PlnDstItem* PlnDstItem::createDstItem(PlnExpression* ex, bool need_save)
{
	PlnDstItem* di = NULL;

	if (ex->type == ET_VALUE) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		int dt = ex->values[0].getVarType()->data_type();
		if (dt == DT_OBJECT_REF) {
			auto asgn_type = ex->values[0].asgn_type;
			if (asgn_type == ASGN_COPY) {
				di = new PlnDstCopyObjectItem(ex);
			} else if (asgn_type == ASGN_MOVE) {
				di = new PlnDstMoveObjectItem(ex);
			} else if (asgn_type == ASGN_COPY_REF) {
				di = new PlnDstPrimitiveItem(ex);
			} else {
				BOOST_ASSERT(false);
			}
		} else if (dt == DT_OBJECT) {
			auto asgn_type = ex->values[0].asgn_type;
			BOOST_ASSERT(asgn_type == ASGN_COPY);
			di = new PlnDstCopyObjectItem(ex);

			//BOOST_ASSERT(false);

		} else {
			di = new PlnDstPrimitiveItem(ex);
		}
	} 

	else if (ex->type == ET_ARRAYITEM) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		int dt = ex->values[0].getVarType()->data_type();
		if (dt == DT_SINT || dt == DT_UINT || dt == DT_FLOAT) {
			di = new PlnDstPrimitiveItem(ex);

		} else if (dt == DT_OBJECT_REF) {
			int at = ex->values[0].asgn_type;
			if (at == ASGN_MOVE) {
				di = new PlnDstMoveIndirectObjItem(ex);
			} else if (at == ASGN_COPY) {
				di = new PlnDstCopyObjectItem(ex);
			} else if (at == ASGN_COPY_REF) {
				di = new PlnDstPrimitiveItem(ex);
			} else {
				BOOST_ASSERT(false);
			}

		} else if (dt == DT_OBJECT) {
			int at = ex->values[0].asgn_type;
			BOOST_ASSERT(at == ASGN_COPY);
			di = new PlnDstCopyObjectItem(ex);
			// BOOST_ASSERT(false);

		} else {
			BOOST_ASSERT(false);
		}

	} else if (ex->type == ET_STRUCTMEMBER) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		int dt = ex->values[0].getVarType()->data_type();
		if (dt == DT_SINT || dt == DT_UINT || dt == DT_FLOAT) {
			di = new PlnDstPrimitiveItem(ex);

		} else {	// dt == DT_OBJECT_REF
			int at = ex->values[0].asgn_type;
			if (at == ASGN_COPY_REF) {
				di = new PlnDstPrimitiveItem(ex);
			} else if (at == ASGN_MOVE) {
				di = new PlnDstMoveIndirectObjItem(ex);
			} else if (at == ASGN_COPY) {
				di = new PlnDstCopyObjectItem(ex);
			} else {
				BOOST_ASSERT(false);
			}
		}
	} else if (ex->type == ET_REFVALUE) {
		int dt = ex->values[0].getVarType()->data_type();
		if (dt == DT_SINT || dt == DT_UINT || dt == DT_FLOAT) {
			di = new PlnDstPrimitiveItem(ex);
		}
	}

	BOOST_ASSERT(di);

	di->need_save = need_save;
	
	return di;
}

