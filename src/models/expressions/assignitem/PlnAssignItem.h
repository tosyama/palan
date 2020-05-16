/// Assignment Item class definition.
///
/// Interface of assignment item classes.
///
/// @file	PlnAssignItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

// PlnAssignItem
class PlnAssignItem {
public:
	virtual void addDstEx(PlnExpression* ex, bool need_save) = 0;
	virtual int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) = 0;

	virtual void finishS(PlnDataAllocator& da, PlnScopeInfo& si) = 0;
	virtual void finishD(PlnDataAllocator& da, PlnScopeInfo& si) = 0;
	virtual void genS(PlnGenerator& g) = 0;
	virtual void genD(PlnGenerator& g) = 0;

	static PlnAssignItem* createAssignItem(PlnExpression* ex);
	virtual ~PlnAssignItem() {};
};

