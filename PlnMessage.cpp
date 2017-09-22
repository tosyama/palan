#include "PlnMessage.h"
#include "boost/format.hpp"

using boost::format;

string PlnMessage::getErr(PlnErrCode err_code, string arg1, string arg2)
{
	string f;
	switch (err_code) {
		case E_UndefinedVariable:
			f  = "Variable '%1%' was not declared in this scope."; break;
		case E_UndefinedFunction:
			f  = "Function '%1%' was not declared in this scope."; break;
		case E_UndefinedType:
			f  = "Type '%1%' was not declared in this scope."; break;
		case E_DuplicateVarName:
			f = "Variable name '%1%' already defined."; break;
		case E_NumOfLRVariables:
			f = "Number of values left and right are not match."; break;
		case E_NumOfRetValues:
			f = "Number of return arguments and definitions are not mutch."; break;
		case E_NeedRetValues:
			f = "Return argument(s) can't be omitted at this function."; break;
		case E_CouldnotOpenFile:
			f = "Could not open file '%1%'."; break;
		default:
			f = "Unknown error.";
	}
	
	string message;
	if (arg1 == "")
		message = f;
	else if (arg2 == "")
		message = (format(f) % arg1).str();
	else
		message = (format(f) % arg1 % arg2).str();

	return  message;
}
