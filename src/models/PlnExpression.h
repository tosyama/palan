/// Expression model class declaration.
///
/// @file	PlnExpression.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

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
	ET_STRUCTMEMBER,
	ET_REFVALUE,
	ET_MCOPY,
	ET_TRUE,
	ET_FALSE,
	ET_VOID
};

class PlnModule;
class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnDataPlace*> data_places;
	vector<PlnValue> values;
	string comment;
	PlnLoc loc;

	PlnExpression(PlnExprsnType type) : type(type) {};
	PlnExpression(PlnValue value);
	virtual ~PlnExpression();

	int getDataType(int val_ind=0);

	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	virtual void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_UNKNOWN,
	VL_LIT_INT8,
	VL_LIT_UINT8,
	VL_LIT_FLO8,
	VL_LIT_STR,
	VL_LIT_ARRAY,
	VL_VAR,
	VL_WORK
};

class PlnArrayValue;
class PlnValue {
public:
	PlnValType type;
	PlnAsgnType asgn_type;
	union {
		int index;
		int64_t intValue;
		uint64_t uintValue;
		double floValue;
		string *strValue;
		PlnArrayValue *arrValue;
		PlnVariable *var;
		PlnVarType *wk_type;
	} inf;

	PlnValue() : type(VL_UNKNOWN), asgn_type(NO_ASGN) {};
	PlnValue(const PlnValue &src);
	PlnValue(int64_t intValue);
	PlnValue(uint64_t uintValue);
	PlnValue(double floValue);
	PlnValue(string strValue);
	PlnValue(PlnArrayValue *arr);
	PlnValue(PlnVariable* var);
	~PlnValue();

	PlnVarType* getVarType();
	PlnDataPlace* getDataPlace(PlnDataAllocator& da);
};
