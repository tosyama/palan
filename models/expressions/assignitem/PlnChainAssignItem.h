/// Assignment Item class definition.
///
/// @file	PlnChainAssignItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnChainAssignItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	struct DstInf {
		PlnDstItem* item;
	};
	vector<DstInf> dsts;

public:
	PlnChainAssignItem(PlnExpression* ex) : src_ex(ex) {
		BOOST_ASSERT(src_ex->type == ET_ASSIGN);
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(ex->values.size() == 1);
		auto v = ex->values[0];

		BOOST_ASSERT(v.type == VL_VAR);
		dsts.push_back( {PlnDstItem::createDstItem(ex, need_save)} );
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) {
		for (auto &di: dsts) {
			di.item->place = data_places[start_ind];
			start_ind++;
			if (start_ind >= data_places.size())
				break;
		}
		return start_ind;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto &di: dsts)
			di.item->setSrcEx(da, si, src_ex);

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto &di: dsts)
			di.item->finish(da, si);
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		for (auto di: dsts)
			di.item->gen(g);
	}
};

