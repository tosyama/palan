/// PlnBoolOperation model class declaration.
///
/// @file	PlnBoolOperation.h
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "PlnCmpOperation.h"

class PlnBoolOperation : public PlnBoolExpression
{
protected:
	PlnBoolExpression *l, *r;
	int end_jmp_id;

public:
	PlnBoolOperation(PlnBoolExpression* l, PlnBoolExpression* r, PlnExprsnType type);
	PlnBoolOperation(const PlnBoolOperation&) = delete;
	~PlnBoolOperation();

	static PlnExpression* create(PlnExpression* l, PlnExpression* r, PlnExprsnType type);
	static PlnExpression* createNot(PlnExpression* e);
};

class PlnAndOperation : public PlnBoolOperation
{
public:
	PlnAndOperation(PlnBoolExpression* l, PlnBoolExpression* r)
		: PlnBoolOperation(l, r, ET_AND) { };
	PlnAndOperation(const PlnAndOperation&) = delete;
	~PlnAndOperation() { };

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

class PlnOrOperation : public PlnBoolOperation
{
public:
	PlnOrOperation(PlnBoolExpression* l, PlnBoolExpression* r)
		: PlnBoolOperation(l, r, ET_OR) { };
	PlnOrOperation(const PlnOrOperation&) = delete;
	~PlnOrOperation() { };

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

