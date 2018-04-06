/// Assignment Item class definition.
///
/// Interface of assignment item classes.
///
/// @file	PlnAssignItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

// PlnAssignItem
class PlnAssignItem {
public:
	virtual bool ready() { return false; }	// Temporary.
	virtual void addDstEx(PlnExpression* ex) { }
	virtual void finishS(PlnDataAllocator& da, PlnScopeInfo& si) { BOOST_ASSERT(false); }
	virtual void finishD(PlnDataAllocator& da, PlnScopeInfo& si) { }
	virtual void genS(PlnGenerator& g) { }
	virtual void genD(PlnGenerator& g) { }

	static PlnAssignItem* createAssignItem(PlnExpression* ex);
};

