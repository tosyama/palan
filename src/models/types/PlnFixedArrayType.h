/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

class PlnFixedArrayTypeInfo : public PlnTypeInfo {
public:
	PlnVarType* item_type;
	bool has_heap_member;
	PlnFunction* alloc_func;
	PlnFunction* internal_alloc_func;
	PlnFunction* free_func;
	PlnFunction* internal_free_func;
	PlnFunction* copy_func;

	PlnFixedArrayTypeInfo(string &name, PlnVarType* item_type, PlnBlock* parent);
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override;

	PlnVarType* getVarType(const string& mode) override;
	PlnVarType* getVarType(const string& mode, vector<PlnExpression*> init_args);
};

class PlnFixedArrayVarType : public PlnVarType {
public:
	vector<int> sizes;
	PlnFixedArrayVarType(PlnTypeInfo* typeinf, const string &mode) : PlnVarType(typeinf, mode) { }
	~PlnFixedArrayVarType() {};

	string tname() override;
	int size() override;
	PlnVarType* getVarType(const string& mode) override;
	PlnTypeConvCap canCopyFrom(PlnVarType *src, PlnAsgnType copymode) override;

	PlnVarType* item_type() {
		auto farr_type = dynamic_cast<PlnFixedArrayTypeInfo*>(typeinf);
		BOOST_ASSERT(farr_type);
		return farr_type->item_type;

	}

	void getAllocArgs(vector<PlnExpression*> &alloc_exps) override;
	PlnExpression* getAllocEx(vector<PlnExpression*> &args) override;
	PlnExpression* getInternalAllocEx(PlnExpression* alloc_var, vector<PlnExpression*> &args) override;
	PlnExpression* getFreeEx(PlnExpression* free_var) override; 
	PlnExpression* getInternalFreeEx(PlnExpression* free_var, vector<PlnExpression*> &args) override; 
	PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var, vector<PlnExpression*> &args) override;
};
