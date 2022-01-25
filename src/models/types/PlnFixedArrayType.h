/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

class PlnFixedArrayTypeInfo : public PlnTypeInfo {
public:
	PlnVarType* item_type;
	bool has_heap_member;
	PlnFunction* alloc_func;

	PlnFixedArrayTypeInfo(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent);
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override;

	vector<PlnVarType*> getAllocParamTypes() override;
	PlnVarType* getVarType(const string& mode) override;
};

class PlnFixedArrayVarType : public PlnVarType {
public:
	vector<int> sizes;
	PlnFixedArrayVarType(PlnTypeInfo* typeinf, const string &mode)
		: PlnVarType(typeinf, mode) 
	{
	}

	PlnVarType* getVarType(const string& mode) override;
	PlnTypeConvCap canCopyFrom(PlnVarType *src, PlnAsgnType copymode) override;

	PlnVarType* item_type() {
		auto farr_type = dynamic_cast<PlnFixedArrayTypeInfo*>(typeinf);
		BOOST_ASSERT(farr_type);
		return farr_type->item_type;

	}

	void getInitExpressions(vector<PlnExpression*> &init_exps) override;
	PlnExpression *getAllocEx(vector<PlnExpression*> &args) override;
};
