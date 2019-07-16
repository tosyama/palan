/// Message definitions.
///
/// Manage output message descriptions in Palan compiler.
///
/// @file	PlnMessage.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <string>
using std::string;

enum PlnErrCode {
	E_UndefinedVariable, // var name
	E_UndefinedFunction,	// func name
	E_NoMatchingFunction,	// func name, candidates func
	E_DuplicateVarName,	// var name
	E_DuplicateFunction,	// func name
	E_NumOfLRVariables,	// none
	E_InvalidRetValues,	// none
	E_NeedRetValues,	// none
	E_InvalidReturnValType, // var name
	E_CouldnotOpenFile,	// file name
	E_CantCopyFreedVar,	// var name
	E_AmbiguousFuncCall,	// func name
	E_IncompatibleTypeAssign,	// src type, dst type
	E_CantUseAtToplevel,	// statement
	E_CantUseMoveOwnership,	// var name
	E_CantDefineConst,	// const name
	E_DuplicateConstName,	// const name
	E_CantUseOperatorHere,	// var name
	E_CantUseIndexHere, // var name
	E_AmbiguousVarType,	// var name
	E_IncompatibleTypeInitVar, // var name
	E_UndefinedConst, // const name
	E_CantUseConstHere, // none
	E_ValueRequired, // none
	E_NotWithInLoop, // none
	E_NoMatchingParameter,	// none

	E_InvalidAST,	// source name, line

	// CUI errors
	E_CUI_NoInputFile,
	E_CUI_IncompatibleOpt,
	E_CUI_InvalidExecOpt,

	// Unsupported grammer
	E_UnsuppotedGrammer // any, any
};

enum PlnHelpCode {
	H_Help,
	H_Version,
	H_Assembly,
	H_Compile,
	H_Output,
	H_Execute,
	H_Input
};

class PlnMessage
{
public:
	// Static terms
	static string floatNumber();
	static string arrayValue();

	static string getErr(PlnErrCode err_code, string arg1="", string arg2="");
	static const char* getHelp(PlnHelpCode help_code);
};
