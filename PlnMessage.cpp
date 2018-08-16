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
		case E_DuplicateVarName:
			f = "Variable name '%1%' already defined."; break;
		case E_DuplicateFunction:
			f = "Function '%1%' already defined."; break;
		case E_NumOfLRVariables:
			f = "Number of left values did not match right values."; break;
		case E_InvalidRetValues:
			f = "Number of return arguments or definitions are not match."; break;
		case E_NeedRetValues:
			f = "Return argument(s) can't be omitted at this function."; break;
		case E_CouldnotOpenFile:
			f = "Could not open file '%1%'."; break;
		case E_CantCopyFreedVar:
			f = "Can not copy to freed variable '%1%'."; break;
		case E_AmbiguousFuncCall:
			f = "Ambiguous function call '%1%'."; break;
		case E_IncompatibleTypeAssign:
			f = "Incompatible types in assignment of '%1%' to '%2%'."; break;
		case E_CantUseAtToplevel:
			f = "Can not use '%1%' at top level code."; break;
		case E_CantUseMoveOwnership:
 			f = "Can not use '>>' for '%1%'."; break;
		case E_CantDefineConst:
			f = "Can not use dynamic expression for const '%1%'."; break;
		case E_DuplicateConstName:
			f = "Const name '%1%' already defined."; break;

		case E_InvalidAST:
			f = "Detected invalid AST at %1%:%2%"; break;
		default:
			return (format("Unknown error. code:%1% - %2%, %3%")  % err_code % arg1 % arg2).str();
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

