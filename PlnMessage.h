/// Message definitions.
///
/// Manage output message descriptions in Palan compiler.
///
/// @file	PlnMessage.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <string>
using std::string;

enum PlnErrCode {
	E_UndefinedVariable, // var name
	E_UndefinedFunction,	// func name
	E_UndefinedType,	// type name
	E_DuplicateVarName,	// var name
	E_NumOfLRVariables,	// none
	E_NumOfRetValues,	// none
	E_NeedRetValues,	// none
	E_CouldnotOpenFile,	// file name
	E_CantUseMoveOwnership, // var name
	E_AmbiguousFuncCall,	// func name
	E_IncompatibleTypeAssign,	// src type, dst type

	E_InvalidAST	// source name, line
};

enum PlnWarnCode {
	W_NumOfLRVariables
};

class PlnMessage
{
public:
	static string getErr(PlnErrCode err_code, string arg1="", string arg2="");
	static string getWarn(PlnWarnCode warn_code, string arg1="", string arg2="");
};
