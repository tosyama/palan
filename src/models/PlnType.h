/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnAllocator {
public:
	virtual PlnExpression* getAllocEx() = 0;
	static PlnExpression* getAllocEx(PlnVariable* var);
};

class PlnFreer {
public:
	virtual PlnExpression* getFreeEx(PlnExpression* free_var) = 0;
	static PlnExpression* getFreeEx(PlnVariable* var);
};

class PlnDeepCopyExpression;
class PlnCopyer {
public:
	virtual PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) = 0;
	virtual PlnDeepCopyExpression* getCopyEx() = 0;
};

enum PlnTypeConvCap {
	TC_CANT_CONV = 0,
	TC_SAME = 1,
	TC_AUTO_CAST = 2,
	TC_LOSTABLE_AUTO_CAST = 4,
	TC_UP_CAST = 8,
	TC_DOWN_CAST = 16,
	TC_CONV_OK = TC_SAME | TC_AUTO_CAST | TC_LOSTABLE_AUTO_CAST | TC_UP_CAST | TC_DOWN_CAST
};

enum PlnTypeType {
	TP_PRIMITIVE,
	TP_FIXED_ARRAY,
	TP_ARRAY_VALUE,
	TP_STRUCT,
	TP_ALIAS
};

class PlnVarType;
class PlnType {
public:
	PlnTypeType type;
	int	data_type;
	string name;
	string default_mode;
	int size;
	union {
		struct {
			bool is_fixed_size;
			int alloc_size;
		} obj;
	} inf;

	vector<PlnVarType*> var_types;

	PlnAllocator *allocator;
	PlnFreer *freer;
	PlnCopyer *copyer;

	struct PlnTypeConvInf {
		PlnType *type;
		PlnTypeConvCap capacity;
		PlnTypeConvInf(PlnType* t, PlnTypeConvCap cap) : type(t), capacity(cap) { }
	};
	vector<PlnTypeConvInf> conv_inf;

	PlnType(PlnTypeType type=TP_PRIMITIVE);
	virtual ~PlnType();
	virtual PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode);

	PlnVarType* getVarType(const string& mode = "---");

	static void initBasicTypes();
	static vector<PlnType*>& getBasicTypes();
	static PlnType* getByte();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getFlo();
	static PlnType* getReadOnlyCStr();
	static PlnType* getObject();
	static PlnType* getAny();

	static string getFixedArrayName(PlnVarType* item_type, vector<int>& sizes);
	static PlnTypeConvCap lowCapacity(PlnTypeConvCap l, PlnTypeConvCap r);
};

class PlnVarType {
public:
	PlnVarType(PlnType* typeinf, const string &mode): typeinf(typeinf), mode(mode) {}
	PlnType* typeinf;
	string mode;

	const string& name() { return typeinf->name; }
	int data_type() { return typeinf->data_type; }
	int size() { return typeinf->size; }
	PlnExpression *getAllocEx() {
		if (!typeinf->allocator) return NULL;
		return typeinf->allocator->getAllocEx();
	}
	PlnExpression *getFreeEx(PlnExpression* free_var) { return typeinf->freer->getFreeEx(free_var); }
	PlnExpression *getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) {
		if (!typeinf->copyer) return NULL;
		return typeinf->copyer->getCopyEx(dst_var, src_var);
	}
	PlnDeepCopyExpression* getCopyEx() {
		PlnCopyer* copyer = typeinf->copyer;
		if (!copyer) return NULL;
		return typeinf->copyer->getCopyEx();
	}
	PlnTypeConvCap canCopyFrom(PlnVarType *src, PlnAsgnType copymode) { return typeinf->canCopyFrom(mode, src, copymode); }
};
