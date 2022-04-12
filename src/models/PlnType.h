/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017-2022 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

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
	string tname;
	string default_mode;

	vector<PlnVarType*> var_types;

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
	virtual vector<PlnVarType*> getFreeParamTypes() { return {}; };
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
	virtual ~PlnVarType() {}

	virtual string tname() { return typeinf->tname; }
	virtual int size();
	int data_type();
	int align();
	bool has_heap_member();

	virtual void getAllocArgs(vector<PlnExpression*> &alloc_exps);
	virtual void getFreeArgs(vector<PlnExpression*> &free_args);

	virtual PlnExpression *getAllocEx(vector<PlnExpression*> &args) {
		BOOST_ASSERT(false); return NULL;
	}

	virtual PlnExpression *getInternalAllocEx(PlnExpression* alloc_var, vector<PlnExpression*> &args) {
		BOOST_ASSERT(false); return NULL;
	}

	virtual PlnExpression *getFreeEx(PlnExpression* free_var, vector<PlnExpression*> &args) {
		BOOST_ASSERT(false); return NULL;
	}

	virtual PlnExpression *getInternalFreeEx(PlnExpression* free_var, vector<PlnExpression*> &free_args) {
		BOOST_ASSERT(false); return NULL;
	}

	virtual PlnExpression *getCopyEx(PlnExpression* dst_var, PlnExpression* src_var, vector<PlnExpression*> &args) {
		return NULL;
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
