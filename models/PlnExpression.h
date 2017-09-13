/// Expression model class declaration.
///
/// @file	PlnExpression.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#pragma once
#include "../PlnModel.h"

class PlnReturnPlace;

// Expression: 
enum PlnExprsnType {
	ET_VALUE,
	ET_MULTI,
	ET_FUNCCALL,
	ET_ADD,
	ET_NEG,
	ET_ASSIGN
};

class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnReturnPlace> ret_places;
	vector<PlnDataPlace*> data_places;
	vector<PlnValue> values;

	PlnExpression(PlnExprsnType type) : type(type) {};
	PlnExpression(PlnValue value);

	virtual void finish(PlnDataAllocator& da);
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_WK_INT8,
	VL_RO_DATA,
	VL_VAR,
};

class PlnValue {
public:
	PlnValType type;
	union {
		int index;
		int64_t intValue;
		PlnReadOnlyData* rod;
		PlnVariable* var;
	} inf;

	PlnValue() {};
	PlnValue(int intValue);
	PlnValue(PlnReadOnlyData* rod);
	PlnValue(PlnVariable* var);

	PlnDataPlace* getDataPlace(PlnDataAllocator& da);
	unique_ptr<PlnGenEntity> genEntity(PlnGenerator& g);
};

enum PlnRtnPlcType {
	RP_NULL,
	RP_TEMP,
	RP_WORK,
	RP_VAR,
	RP_AS_IS,
	RP_ARGPLN,
	RP_ARGSYS
};

class PlnReturnPlace
{
public:
	PlnRtnPlcType type;
	union {
		struct {
			int32_t index;
			int32_t size;
		} arg;
		PlnVariable* var;
		PlnValue* as_is;
		struct {
			int index;
		} wk;
	}	inf;

	string commentStr();
	void dump(ostream& os, string indent="");
	unique_ptr<PlnGenEntity> genEntity(PlnGenerator& g);
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
	unique_ptr<PlnGenEntity> genEntity(PlnGenerator& g);
};
