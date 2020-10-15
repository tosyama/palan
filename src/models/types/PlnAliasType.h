/// Alias type class declaration.
///
/// @file	PlnAliasType.h
/// @copyright	2019-2020 YAMAGUCHI Toshinobu 

class PlnAliasType : public PlnType {
public:
	PlnType* orig_type;
	PlnAliasType(const string &name, PlnType* orig_type)
		: PlnType(TP_ALIAS), orig_type(orig_type)
	{
		this->name = name;
		this->data_type = orig_type->data_type;
		this->size = orig_type->size;
	}

	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override { BOOST_ASSERT(false); }
};
