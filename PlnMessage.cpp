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
		case E_InvalidReturnValType:
			f = "Return value '%1%' type need to same as the parameter type."; break;
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
		case E_CantUseOperatorHere:
			f = "Can not use the operator for '%1%'."; break;
		case E_CantUseIndexHere:
			f = "Can not use the index operator for '%1%'."; break;
		case E_CantUseDynamicValue:
			f = "Can not use dynamic expression for '%1%'."; break;
		case E_AllowedOnlyInteger:
			f = "Only allowed to use integer here."; break;
		case E_AmbiguousVarType:
			f = "Type of variable '%1%' is ambiguous."; break;
		case E_IncompatibleTypeInitVar:
			f = "Incompatible type to init variable '%1%'."; break;
		case E_UndefinedConst:
			f  = "Constant '%1%' was not declared in this scope."; break;

		case E_InvalidAST:
			f = "Detected invalid AST at %1%:%2%"; break;

		case E_CUI_NoInputFile:
			f = "No input file"; break;
		case E_CUI_IncompatibleOpt:
			f = "Incompatible options are specifiled"; break;
		case E_CUI_InvalidExecOpt:
			f = "Excecute option use only with output option"; break;

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

const char* PlnMessage::getHelp(PlnHelpCode help_code)
{
	switch (help_code) {
		case H_Help:
			return "Display this help";
		case H_Version:
			return "Display compiler version";
		case H_Assembly:
			return "Display compiled assembly";
		case H_Compile:
			return "Compile, assemble and output object file";
		case H_Output:
			return "Output executable file";
		case H_Execute:
			return "Execute immediately after output executable file";
		case H_Input:
			return "Specify input palan source file";
	}
	BOOST_ASSERT(false);
}

string PlnMessage::floatNumber()
{
	return "float number";
}

string PlnMessage::arrayValue()
{
	return "array value";
}
