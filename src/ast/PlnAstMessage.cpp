/// Message definitions.
///
/// Manage output message descriptions in Palan parser.
///
/// @file	PlnAstMessage.cpp
/// @copyright	2021 YAMAGUCHI Toshinobu 

#include "PlnAstMessage.h"
#include "boost/format.hpp"

using boost::format;

string PlnAstMessage::getErr(PlnErrCode err_code, string arg1, string arg2)
{
	string f;
	switch (err_code) {
		case E_CouldnotOpenFile:
			f = "Could not open file '%1%'."; break;
		default:
			BOOST_ASSERT(false);
	}
	
	string message;
	if (arg1 == "\x01")
		message = f;
	else if (arg2 == "\x01")
		message = (format(f) % arg1).str();
	else
		message = (format(f) % arg1 % arg2).str();

	return  message;
}

const char* PlnAstMessage::getHelp(PlnHelpCode help_code)
{
	switch (help_code) {
		case H_Help:
			return "Display this help";
		case H_Output:
			return "Output AST json file";
		case H_Indent:
			return "Output Indented json";
		case H_Input:
			return "Input palan file";
	}
	BOOST_ASSERT(false);
}
