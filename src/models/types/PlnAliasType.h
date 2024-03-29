/// Alias type class declaration.
///
/// @file	PlnAliasType.h
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

class PlnAliasTypeInfo : public PlnTypeInfo {
public:
	PlnVarType* orig_type;
	PlnAliasTypeInfo(const string &name, PlnVarType* orig_type, PlnTypeInfo* orig_type2)
		: PlnTypeInfo(TP_ALIAS), orig_type(orig_type)
	{
		this->tname = name;
		this->data_type = orig_type->typeinf->data_type;
		this->data_size = orig_type->typeinf->data_size;
		this->data_align = orig_type->typeinf->data_align;
	}
	
	// LCOV_EXCL_START
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override {
		BOOST_ASSERT(false);
	}
	// LCOV_EXCL_STOP
};
