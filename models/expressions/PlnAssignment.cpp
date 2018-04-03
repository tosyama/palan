/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "PlnAssignment.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"
#include "../PlnHeapAllocator.h"
#include "PlnDivOperation.h"

// PlnDstItem
class PlnDstItem {
public:
	virtual bool ready() { return false; }	// Temporary.

	virtual PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) { return NULL; }
	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si) { BOOST_ASSERT(false); }
	virtual void gen(PlnGenerator& g) { }
};

// PlnDstPrimitiveItem
class PlnDstPrimitiveItem : public PlnDstItem {
	PlnExpression *dst_ex;
	PlnDataPlace* dst_dp;

public:
	PlnDstPrimitiveItem(PlnExpression* ex) : dst_ex(ex), dst_dp(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type == NO_PTR);
	}

	bool ready() override { return true; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		return dst_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_dp->src_place);
		da.popSrc(dst_dp);
		dst_ex->finish(da, si);
	}

	void gen(PlnGenerator& g) override {
		g.genLoadDp(dst_dp);
		dst_ex->gen(g);
	}
};

PlnDstItem* createDstItem(PlnExpression* ex)
{
	if (ex->type == ET_VALUE) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		PlnType* t = ex->values[0].inf.var->var_type.back();
		if (t->data_type != DT_OBJECT_REF) {
			return new PlnDstPrimitiveItem(ex);
		}
	} 

	return new PlnDstItem();
}

// PlnAssignItem
class PlnAssignItem {
	vector<PlnDataPlace*>	data_places;

public:
	virtual bool ready() { return false; }	// Temporary.
	virtual void addDstEx(PlnExpression* ex) { }
	virtual void finishS(PlnDataAllocator& da, PlnScopeInfo& si) { BOOST_ASSERT(false); }
	virtual void finishD(PlnDataAllocator& da, PlnScopeInfo& si) { }
	virtual void genS(PlnGenerator& g) { }
	virtual void genD(PlnGenerator& g) { }
};


// PlnAssignPrimitiveItem
// Src value don't need to clear.
class PlnAssignPrimitiveItem : public PlnAssignItem {
	PlnExpression* src_ex;
	PlnDstItem* dst_item;

public:
	PlnAssignPrimitiveItem(PlnExpression* ex) : src_ex(ex), dst_item(NULL) {
	}
	
	bool ready() override { return dst_item->ready(); };

	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type == NO_PTR);
		dst_item = createDstItem(ex);
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		src_ex->data_places.push_back(dst_item->getInputDataPlace(da));
		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da, si);
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		dst_item->gen(g);
	}
};

// PlnAssignWorkValsItem
// Src that return multiple work values expression.
// The work valuses don't need to clear.
// Only case need to free is after object was copied.
class PlnAssignWorkValsItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	vector<PlnDstItem*> dst_items;
public:
	PlnAssignWorkValsItem(PlnExpression* ex) : src_ex(ex) {
	}

	bool ready() override {
		for(auto di: dst_items)
			if (!di->ready()) return false;
		return true;
	};

	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		dst_items.push_back(createDstItem(ex));
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto dst_item: dst_items)
			src_ex->data_places.push_back(dst_item->getInputDataPlace(da));
		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto dst_item: dst_items)
			dst_item->finish(da, si);
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		for (auto dst_item: dst_items)
			dst_item->gen(g);
	}
};

PlnAssignItem* createAssignItem(PlnExpression* ex)
{
	if (ex->type == ET_VALUE) {
		PlnValue v = ex->values[0];
		if (v.type == VL_LIT_INT8 || v.type == VL_LIT_UINT8 || v.type == VL_RO_DATA) {
			return new PlnAssignPrimitiveItem(ex);
		}
		if (v.type == VL_VAR) {
			int dt = ex->values[0].inf.var->var_type.back()->data_type;
			if (dt == DT_SINT || dt == DT_UINT) {
				return new PlnAssignPrimitiveItem(ex);
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

// ==========================
// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnExpression*>& lvals, vector<PlnExpression*>& exps)
	: lvals(move(lvals)), PlnExpression(ET_ASSIGN), expressions(move(exps))
{
	for (auto e: this->lvals) {
		BOOST_ASSERT(e->values.size() == 1);
		BOOST_ASSERT(e->type == ET_VALUE || e->type == ET_ARRAYITEM);
		BOOST_ASSERT(e->values[0].type == VL_VAR);
		values.push_back(e->values[0]);
	}

	int dst_i = 0;
	for (auto ex: expressions) {
		PlnAssignItem* ai = createAssignItem(ex);
		for (int i=0; i<ex->values.size(); ++i) {
			if (dst_i < this->lvals.size()) {
				ai->addDstEx(this->lvals[dst_i]);
				dst_i++;
			}
		}
		assgin_items.push_back(ai);
	}
}

enum {	// AssignInf.type
	AI_PRIMITIVE,
	AI_MCOPY,
	AI_MOVE,
	AI_ASSIGN_ITEM
};

inline union PlnAssignInf getDeepCopyAssignInf(PlnDataAllocator& da, PlnValue& dst_val, PlnValue& src_val)
{
	union PlnAssignInf as_inf;

	as_inf.type = AI_MCOPY;
	da.prepareMemCopyDps(as_inf.mcopy.dst, as_inf.mcopy.src);

	// If src is work objects, should be free after copied. 
	if (src_val.type == VL_WORK)
		as_inf.mcopy.free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
	else
		as_inf.mcopy.free_dp = NULL;

	PlnType *t = dst_val.getType();
	if (t->inf.obj.is_fixed_size) 
		as_inf.mcopy.cp_size = t->inf.obj.alloc_size;
	else BOOST_ASSERT(false);

	return as_inf;
}

// Case1) svar ->> dvar
//		1.free dvar, 2.move svar to dvar, 3.clear svar
// Case2) sarr[x] ->> dvar
//		1.calc saddr, 2.save sarr[] 3.free dvar, 4.move save to dvar, 5.clear save sarr[]
// Case3-1) sfunc(z) ->> dvar 
//		1.exec func, 2. save ret, 3. free dvar, 4.move save to dvar.
// Case3-2) a+1 ->> dvar 
//		1.calc value , 2. save value, 3. free dvar, 4.move save to dvar.
// Case4) svar ->> darr[y]
//		1.calc darr addr, 2.save darr[], 3. free darr[], 4. move svar to save darr[] 5. clear svar
// Case5) sarr[x] ->> darr[y] 
//		1. calc sarr addr, 2. save sarr[] addr,  3. calc darr addr, 4. save darr addr
//		5. free save darr, 6. move save sarr to save darr, 6. clear save sarr
// Case6) sfunc(z) ->> darr[y]
//		1.exec func, 2. save ret, 3. calc darr addr, 4.save darr addr. 3. free dvar addr
//		4.move saveed red to save darr.
//
// CaseX) sarr ->> darr[]:not alloc

inline union PlnAssignInf getMoveOwnerAssignInf(PlnDataAllocator& da, PlnScopeInfo& si, PlnValue& dst_val, PlnValue& src_val)
{
	union PlnAssignInf as_inf;

	as_inf.type = AI_MOVE;
	as_inf.move.dst = dst_val.inf.var->place;

	if (src_val.type == VL_WORK) {
		// work var
		static string cmt = "save";
		as_inf.move.src = new PlnDataPlace(8, DT_OBJECT_REF);
		as_inf.move.src->comment = &cmt;
		as_inf.move.do_clear_var = NULL; 
		as_inf.move.save_indirect = NULL;

	} else {
		// normal var
		BOOST_ASSERT(src_val.type == VL_VAR);
		as_inf.move.src = src_val.inf.var->place;
		as_inf.move.do_clear_var = src_val.inf.var;

		if (dst_val.inf.var->ptr_type & PTR_INDIRECT_ACCESS) {
			auto save_dp = new PlnDataPlace(8, DT_OBJECT_REF);
			auto base_dp = da.prepareObjBasePtr();

			da.setIndirectObjDp(save_dp, base_dp, NULL);
			as_inf.move.save_indirect = save_dp;

		} else {
			as_inf.move.save_indirect = NULL;
		}
	}

	auto lt = si.get_lifetime(dst_val.inf.var);

	if (lt == VLT_ALLOCED || lt == VLT_INITED) {
//	if (true) {
		as_inf.move.free_dst = PlnHeapAllocator::createHeapFree(dst_val.inf.var);
		BOOST_ASSERT(as_inf.move.free_dst);

	} else {
		as_inf.move.free_dst = NULL;
	}

	return as_inf;
}

inline PlnDataPlace* prepareAssignInf(PlnDataAllocator& da, PlnScopeInfo& si, union PlnAssignInf* as_inf, PlnValue& dst_val, PlnValue& src_val)
{
	if (dst_val.inf.var->ptr_type == NO_PTR) {
		as_inf->type = AI_PRIMITIVE;
		as_inf->inf.dp = dst_val.getDataPlace(da);
		return as_inf->inf.dp;

	}
	
	// (ptr_type & PTR_REFERNCE)
	BOOST_ASSERT(dst_val.getType()->data_type == DT_OBJECT_REF);
	BOOST_ASSERT(src_val.getType()->data_type == DT_OBJECT_REF);

	PlnDataPlace *ret_dp;
	switch (dst_val.lval_type) {
		case LVL_COPY:
			*as_inf = getDeepCopyAssignInf(da, dst_val, src_val);
			ret_dp = as_inf->mcopy.src;
			break;

		case LVL_MOVE: // Move ownership: Free dst var -> Copy address(src->dst) -> Clear src (if not work var)
			*as_inf = getMoveOwnerAssignInf(da, si, dst_val, src_val);
			ret_dp = as_inf->move.src;
			break;

		case LVL_REF:
			as_inf->type = AI_PRIMITIVE;
			as_inf->inf.dp = dst_val.getDataPlace(da);
			ret_dp = as_inf->inf.dp;
			break;	

		default: BOOST_ASSERT(false);
	}
	return ret_dp;
}

void PlnAssignment::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// old;
	int vcnt=0;
	int vi=0;
	// for (auto e: expressions) {
	int ei=0;
	for (auto ai: assgin_items) {
		PlnExpression* e = expressions[ei++];

		if (ai->ready() ) {
			ai->finishS(da, si);
			ai->finishD(da, si);

			for (auto src_val: e->values) {
				union PlnAssignInf as_inf;
				as_inf.type = AI_ASSIGN_ITEM;
				assign_inf.push_back(as_inf);
			}
			vcnt += e->values.size();

		} else {
			for (auto src_val: e->values) {
				if (vcnt >= values.size()) break;
				auto& dst_val = values[vcnt];
				BOOST_ASSERT(dst_val.type == VL_VAR);

				union PlnAssignInf as_inf;
				auto dst_dp = prepareAssignInf(da, si, &as_inf, dst_val, src_val);
				e->data_places.push_back(dst_dp);
				assign_inf.push_back(as_inf);

				vcnt++;
			}

			e->finish(da, si);

			// for save returned reference.
			for (int i=vi; i<vcnt; i++) {
				auto &as_inf = assign_inf[i];
				switch (as_inf.type) {
					case AI_PRIMITIVE:
						break;
					case AI_MCOPY: 
						da.allocDp(as_inf.mcopy.src);	// would be released by memCopyed.
						break;

					case AI_MOVE:
						if (!as_inf.move.do_clear_var) 
							da.allocData(as_inf.move.src); // for save work.
						break;
				}
			}

			for (int i=vi; i<vcnt; i++)
				lvals[i]->finish(da, si);

			// assign process
			for (int i=vi; i<vcnt; i++) {
				auto& as_inf = assign_inf[i];
				switch(as_inf.type) {
					case AI_PRIMITIVE:
						da.popSrc(as_inf.inf.dp);
						break;

					case AI_MCOPY:
						da.popSrc(as_inf.mcopy.src);
						da.allocDp(as_inf.mcopy.dst);	// would be released by memCopyed.
						if (as_inf.mcopy.free_dp) {
							da.allocData(as_inf.mcopy.free_dp);
							da.memCopyed(as_inf.mcopy.dst, as_inf.mcopy.src);
							da.memFreed();
							da.releaseData(as_inf.mcopy.free_dp);
						} else 
							da.memCopyed(as_inf.mcopy.dst, as_inf.mcopy.src);
						break;

					case AI_MOVE:
						da.popSrc(as_inf.move.src);
						if (as_inf.move.save_indirect) {
							da.allocDp(as_inf.move.save_indirect->data.indirect.base_dp);
							if (as_inf.move.free_dst)
								as_inf.move.free_dst->finish(da, si);
							// da.memFreed();
							da.releaseData(as_inf.move.save_indirect->data.indirect.base_dp);
						} else {
							if (as_inf.move.free_dst)
								as_inf.move.free_dst->finish(da, si);
							// da.memFreed();
						}

						if (auto var = as_inf.move.do_clear_var) {
							if (si.exists_current(var))
								si.set_lifetime(var, VLT_FREED);
						} else {
							da.releaseData(as_inf.move.src);
						}

						auto dst_var = values[i].inf.var;
						if (si.exists_current(dst_var))
							si.set_lifetime(dst_var, VLT_INITED);

						break;
				}
			}
		}
		vi=vcnt;
	}
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	for (auto e: expressions)
		e->dump(os, indent+" ");	
}

static void genDeepCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp);
static void genMoveOwner4Assign(PlnGenerator &g, PlnExpression *lval_ex, PlnAssignInf &as);

void PlnAssignment::gen(PlnGenerator& g)
{
	int vi=0;
	int vcnt = 0;
	int i = 0;
	//for (auto e: expressions) {
	for (auto ai: assgin_items) {
		PlnExpression* e = expressions[i++];
		if (ai->ready()) {
			g.comment("vv== new start ===");
			ai->genS(g);
			ai->genD(g);
			g.comment("^^== new end ===");

			vcnt += e->values.size();
			vi = vcnt;
			continue;
		}

		g.comment("vv== old start ===");
		
		vcnt += e->values.size();
		if (vcnt > lvals.size()) vcnt = lvals.size();

		e->gen(g);

		for (int di=0; vi < vcnt; vi++, di++) {
			switch (assign_inf[vi].type) {
				case AI_PRIMITIVE:
					lvals[vi]->gen(g);
					g.genLoadDp(assign_inf[vi].inf.dp);
					break;

				case AI_MCOPY:
					lvals[vi]->gen(g);
					genDeepCopy4Assign(g, assign_inf[vi], values[vi].inf.var->place);
					break;

				case AI_MOVE:
					genMoveOwner4Assign(g, lvals[vi], assign_inf[vi]);
					break;
				default:
					BOOST_ASSERT(false);
			}
		}
		g.comment("^^== old end ===");
	}
	PlnExpression::gen(g);
}

void genDeepCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp)
{
	auto as_inf = as.mcopy;
	g.genLoadDp(as_inf.src);

	auto cpy_dste = g.getEntity(as_inf.dst);
	auto cpy_srce = g.getEntity(as_inf.src);

	// save src(work object) for free
	if (as_inf.free_dp) {
		auto fe = g.getEntity(as_inf.free_dp);
		g.genMove(fe.get(), cpy_srce.get(), *as_inf.src->comment + " -> save for free");
	}

	// get dest
	auto ve = g.getEntity(var_dp);
	g.genMove(cpy_dste.get(), ve.get(), *var_dp->comment + " -> " + *as_inf.dst->comment);

	// copy
	static string cmt = "deep copy";
	g.genMemCopy(as_inf.cp_size, cmt);

	// free work object
	if (as_inf.free_dp) {
		static string cmt = "work";
		auto fe = g.getEntity(as_inf.free_dp);
		g.genMemFree(fe.get(), cmt, false);
	}
}

void genMoveOwner4Assign(PlnGenerator &g, PlnExpression *lval_ex, PlnAssignInf &as)
{
	auto as_inf = as.move;

	g.genLoadDp(as_inf.src);

	lval_ex->gen(g);
	auto dste = g.getEntity(as_inf.dst);
	auto srce = g.getEntity(as_inf.src);

	// Free losted owner variable.
	static string cmt = "lost ownership ";
	if (as_inf.save_indirect && as_inf.free_dst) {
		// TODO: Use genLoadDp.
		auto savei_base_e = g.getEntity(as_inf.save_indirect->data.indirect.base_dp);
		g.genLoadAddress(savei_base_e.get(), dste.get(), "save address of " + as_inf.dst->cmt());

		if (as_inf.free_dst)
			as_inf.free_dst->gen(g);

		// assign value.
		auto savei_e = g.getEntity(as_inf.save_indirect);
		g.genMove(savei_e.get(), srce.get(), *as_inf.src->comment + " -> " + *as_inf.dst->comment);

	} else {
		if (as_inf.free_dst)
			as_inf.free_dst->gen(g);

		// assign value.
		g.genMove(dste.get(), srce.get(), *as_inf.src->comment + " -> " + *as_inf.dst->comment);
	}

	if (as_inf.do_clear_var) {
		vector<unique_ptr<PlnGenEntity>> clr_es;
		clr_es.push_back(g.getEntity(as_inf.src));
		g.genNullClear(clr_es);
	}
}
