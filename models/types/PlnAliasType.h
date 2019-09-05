/// Alias type class declaration.
///
/// @file	PlnAliasType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

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

	PlnTypeConvCap canConvFrom(PlnType *src) override
	{ BOOST_ASSERT(false); }
};
