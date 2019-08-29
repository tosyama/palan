/// Expression model class declaration.
///
/// @file	PlnExpression.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
	ET_MCOPY
};

class PlnModule;
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

	virtual PlnExpression* adjustTypes(const vector<PlnType*> &types);
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
	VL_UNKNOWN,
	VL_LIT_INT8,
	VL_LIT_UINT8,
	VL_LIT_FLO8,
	VL_LIT_STR,
	VL_LIT_ARRAY,
	VL_VAR,
	VL_WORK
};

enum PlnAsgnType {
	NO_ASGN,
	ASGN_COPY,
	ASGN_MOVE,
	ASGN_COPY_REF
};

class PlnArrayLiteral;
class PlnArrayValue;
class PlnValue {
public:
	PlnValType type;
	PlnAsgnType asgn_type;
	bool is_readonly;	// if true, you should not update.
	bool is_cantfree;	// if true, you should not free.
	union {
		int index;
		int64_t intValue;
		uint64_t uintValue;
		double floValue;
		string *strValue;
		PlnArrayValue *arrValue;
		PlnVariable *var;
		PlnType *wk_type;
	} inf;

	PlnValue() : type(VL_UNKNOWN), asgn_type(NO_ASGN), is_readonly(false)  {};
	PlnValue(const PlnValue &src);
	PlnValue(int64_t intValue);
	PlnValue(uint64_t uintValue);
	PlnValue(double floValue);
	PlnValue(string strValue);
	PlnValue(PlnArrayValue *arr);
	PlnValue(PlnArrayLiteral *arr);
	PlnValue(PlnVariable* var);
	~PlnValue();

	PlnType* getType();
	PlnDataPlace* getDataPlace(PlnDataAllocator& da);
};
