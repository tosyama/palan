/// Calculate operation utilities.
///
/// @file	PlnCalcOperationUtls.h
/// @copyright	2021 YAMAGUCHI Toshinobu 

#define CREATE_CHECK_FLAG(ex)	bool is_##ex##_int = false, is_##ex##_uint = false, is_##ex##_flo = false;	\
	union {int64_t i; uint64_t u; double d;} ex##val; \
	if (ex->type == ET_VALUE) { \
		switch (ex->values[0].type) { \
			case VL_LIT_INT8: is_##ex##_int = true; \
				ex##val.i = ex->values[0].inf.intValue; break;\
			case VL_LIT_UINT8: is_##ex##_uint = true; \
				ex##val.u = ex->values[0].inf.uintValue; break; \
			case VL_LIT_FLO8: is_##ex##_flo = true; \
				ex##val.d = ex->values[0].inf.floValue; break; \
		} \
	} \
	bool is_##ex##_num_lit = is_##ex##_int || is_##ex##_uint || is_##ex##_flo;

inline PlnVarType* binaryOperationType(PlnExpression* l, PlnExpression* r)
{
	int ldtype = l->getDataType();
	int rdtype = r->getDataType();
	bool isUnsigned = (ldtype == DT_UINT && rdtype == DT_UINT);
	bool isFloat = (ldtype == DT_FLOAT || rdtype == DT_FLOAT);

	if (isFloat) {
		int fsize = 0;
		if (ldtype == DT_FLOAT && rdtype != DT_FLOAT) {
			fsize = l->values[0].getVarType()->size();
		} else if (ldtype != DT_FLOAT && rdtype == DT_FLOAT) {
			fsize = r->values[0].getVarType()->size();
		} else if (l->values[0].type == VL_LIT_FLO8) {
			fsize = r->values[0].getVarType()->size();
		} else if (r->values[0].type == VL_LIT_FLO8) {
			fsize = l->values[0].getVarType()->size();
		} else {
			fsize = std::max(
					l->values[0].getVarType()->size(),
					r->values[0].getVarType()->size());
		}

		if (fsize == 8) {
			return PlnVarType::getFlo64();
		} else if (fsize == 4) {
			return PlnVarType::getFlo32();
		} else
			BOOST_ASSERT(false);

	} else if (isUnsigned) {
		return PlnVarType::getUint();
	}

	return PlnVarType::getSint();
}
