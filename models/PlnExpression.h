/// Expression model class declaration.
///
/// @file	PlnExpression.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#pragma once
#include "../PlnModel.h"

// Expression: 
enum PlnExprsnType {
	ET_VALUE,
	ET_FUNCCALL,
	ET_ADD,
	ET_MUL,
	ET_DIV,
	ET_NEG,
	ET_CMP,
	ET_ASSIGN,
	ET_MVOWN,
	ET_CLONE,
	ET_ARRAYITEM
};

class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnDataPlace*> data_places;
	vector<PlnValue> values;
	PlnDataPlace* val_place;	// TODO: move this member to PlnValue expression.

	PlnExpression(PlnExprsnType type) : type(type), val_place(NULL) {};
	PlnExpression(PlnValue value);

	int getDataType(int val_ind=0);
	bool isLitNum(int& num_type);

	virtual void finish(PlnDataAllocator& da);
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_LIT_UINT8,
	VL_RO_DATA,
	VL_VAR,
	VL_WORK
};

enum PlnLValType {
	NO_LVL,
	LVL_COPY,
	LVL_MOVE,
	LVL_REF
};

class PlnValue {
public:
	PlnValType type;
	PlnLValType lval_type;
	union {
		int index;
		int64_t intValue;
		uint64_t uintValue;
		PlnReadOnlyData* rod;
		PlnVariable* var;
		PlnType* wk_type;
	} inf;

	PlnValue() {};
	PlnValue(int64_t intValue);
	PlnValue(uint64_t uintValue);
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
