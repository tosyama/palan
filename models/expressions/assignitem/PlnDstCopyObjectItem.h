/// Assignment Item class definition.
///
/// A single Dst variables of object reference (not indirect access).
/// for deep copy. 
///
/// @file	PlnDstCopyObjectItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstCopyObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex, *cpy_ex;

public:
	PlnDstCopyObjectItem(PlnExpression* ex) : dst_ex(ex), cpy_ex(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
	}

	PlnAsgnType getAssginType() override { return ASGN_COPY; }

	void setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		PlnType *t = dst_ex->values[0].getType();
		auto copyer = t->copyer;
		cpy_ex = copyer->getCopyEx(dst_ex, src_ex);
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		PlnVariable* var = dst_ex->values[0].inf.var;
		if (si.get_lifetime(var) == VLT_FREED) {
			PlnCompileError err(E_CantCopyFreedVar, var->name);
			err.loc = dst_ex->loc;
			throw err;
		}

		cpy_ex->finish(da, si);
		if (place) {
			if (dst_ex->type == ET_VALUE) {
				PlnDataPlace *dp = dst_ex->values[0].getDataPlace(da);
				da.pushSrc(place, dp);
			} else {
				BOOST_ASSERT(false);
			}
		}
	}

	void gen(PlnGenerator& g) override {
		cpy_ex->gen(g);
		if (place) {
			g.genSaveDp(place);
		}
	}
};

