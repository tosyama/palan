/// Expression model class declaration.
///
/// @file	PlnExpression.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#pragma once
#include "../PlnModel.h"

// Expression: 
enum PlnExprsnType {
	ET_VALUE,
	ET_ARRAYVALUE,
	ET_FUNCCALL,
	ET_ADD,
	ET_MUL,
	ET_DIV,
	ET_NEG,
	ET_CMP,
	ET_AND,
	ET_OR,
	ET_ASSIGN,
	ET_CHAINCALL,
	ET_CLONE,
	ET_ARRAYITEM,
	ET_MCOPY
};

class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnDataPlace*> data_places;
	vector<PlnValue> values;
	PlnLoc loc;

	PlnExpression(PlnExprsnType type) : type(type) {};
	PlnExpression(PlnValue value);
	virtual ~PlnExpression();

	int getDataType(int val_ind=0);

	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	virtual void gen(PlnGenerator& g);
};

class PlnDeepCopyExpression : public PlnExpression {
public:
	PlnDeepCopyExpression(PlnExprsnType type) : PlnExpression(type) {};
	virtual PlnDataPlace* dstDp(PlnDataAllocator &da) = 0;
	virtual PlnDataPlace* srcDp(PlnDataAllocator &da) = 0;
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_LIT_UINT8,
	VL_LIT_FLO8,
	VL_RO_DATA,
	VL_VAR,
	VL_WORK
};

enum PlnAsgnType {
	NO_ASGN,
	ASGN_COPY,
	ASGN_MOVE,
	ASGN_COPY_REF
};

class PlnValue {
public:
	PlnValType type;
	PlnAsgnType asgn_type;
	union {
		int index;
		int64_t intValue;
		uint64_t uintValue;
		double floValue;
		PlnReadOnlyData* rod;
		PlnVariable* var;
		vector<PlnType*> *wk_type;
	} inf;

	PlnValue() {};
	PlnValue(int64_t intValue);
	PlnValue(uint64_t uintValue);
	PlnValue(double floValue);
	PlnValue(PlnReadOnlyData* rod);
	PlnValue(PlnVariable* var);

	PlnType* getType();
	PlnDataPlace* getDataPlace(PlnDataAllocator& da);
};

// Read only data (String literal/Const)
enum PlnRodType {
	RO_LIT_STR
};

class PlnReadOnlyData {
public:
	PlnRodType type;
	int index;
	string name;
	void gen(PlnGenerator& g);
};
