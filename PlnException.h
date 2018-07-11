/// Exception difinition
///
/// @file	PlnException.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnCompileError : public std::exception {
public:
	PlnErrCode err_code;
	string arg1, arg2;
	PlnLoc loc;

	PlnCompileError(PlnErrCode err_code, string arg1="", string arg2="")
		: err_code(err_code), arg1(arg1), arg2(arg2)
	{}
};

