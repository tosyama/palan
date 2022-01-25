/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017-2022 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnAllocator {
public:
	virtual PlnExpression* getAllocEx(vector<PlnExpression*>& args) = 0;
	static PlnExpression* getAllocEx(PlnVariable* var);
};

class PlnInternalAllocator {
public:
	virtual PlnExpression* getInternalAllocEx(PlnExpression* base_var) = 0;
	static PlnExpression* getInternalAllocEx(PlnVariable* var);
};

class PlnFreer {
public:
	virtual PlnExpression* getFreeEx(PlnExpression* free_var) = 0;
	static PlnExpression* getFreeEx(PlnVariable* var);
	static PlnExpression* getInternalFreeEx(PlnVariable* var);
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
class PlnTypeInfo {

public:
	PlnTypeType type;
	int	data_type;
	int data_size;
	int data_align;
	string name;
	string default_mode;

	vector<PlnVarType*> var_types;

	PlnAllocator *allocator;
	PlnInternalAllocator *internal_allocator;
	PlnFreer *freer;
	PlnFreer *internal_freer;
	PlnCopyer *copyer;

	struct PlnTypeConvInf {
		PlnTypeInfo *type;
		PlnTypeConvCap capacity;
		PlnTypeConvInf(PlnTypeInfo* t, PlnTypeConvCap cap) : type(t), capacity(cap) { }
	};
	vector<PlnTypeConvInf> conv_inf;

	PlnTypeInfo(PlnTypeType type=TP_PRIMITIVE);
	virtual ~PlnTypeInfo();
	virtual PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode);

	virtual vector<PlnVarType*> getAllocParamTypes() { return {}; };
	virtual PlnVarType* getVarType(const string& mode = "---");

	static void initBasicTypes();
	static vector<PlnTypeInfo*>& getBasicTypes();

	static string getFixedArrayName(PlnVarType* item_type, vector<int>& sizes);
	static PlnTypeConvCap lowCapacity(PlnTypeConvCap l, PlnTypeConvCap r);
};

class PlnVarType {
public:
	PlnTypeInfo* typeinf;
	string mode;

	PlnVarType(PlnTypeInfo* typeinf, const string &mode): typeinf(typeinf), mode(mode) {}

	const string& name() { return typeinf->name; }
	int data_type();
	int size();
	int align();
	bool has_heap_member();

	virtual void getInitExpressions(vector<PlnExpression*> &init_exps);

	virtual PlnExpression *getAllocEx(vector<PlnExpression*> &args) {
		if (!typeinf->allocator) return NULL;
		vector<PlnExpression*> args0;
		return typeinf->allocator->getAllocEx(args0);
	}

	PlnExpression *getInternalAllocEx(PlnExpression *base_var) {
		if (!typeinf->internal_allocator) return NULL;
		return typeinf->internal_allocator->getInternalAllocEx(base_var);
	}

	PlnExpression *getFreeEx(PlnExpression* free_var) { return typeinf->freer->getFreeEx(free_var); }
	PlnExpression *getInternalFreeEx(PlnExpression* free_var) {
		if (!typeinf->internal_freer) return NULL;
		return typeinf->internal_freer->getFreeEx(free_var);
	}
	PlnExpression *getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) {
		if (!typeinf->copyer) return NULL;
		return typeinf->copyer->getCopyEx(dst_var, src_var);
	}
	PlnDeepCopyExpression* getCopyEx() {
		PlnCopyer* copyer = typeinf->copyer;
		if (!copyer) return NULL;
		return typeinf->copyer->getCopyEx();
	}

	virtual PlnVarType* getVarType(const string& mode = "---");
	virtual PlnTypeConvCap canCopyFrom(PlnVarType *src, PlnAsgnType copymode) { return typeinf->canCopyFrom(mode, src, copymode); }

	static PlnVarType* getByte(const string& mode = "---");
	static PlnVarType* getSint(const string& mode = "---");
	static PlnVarType* getUint(const string& mode = "---");
	static PlnVarType* getFlo32(const string& mode = "---");
	static PlnVarType* getFlo64(const string& mode = "---");
	static PlnVarType* getReadOnlyCStr(const string& mode = "---");
	static PlnVarType* getObject(const string& mode = "---");
	static PlnVarType* getAny(const string& mode = "---");
};
