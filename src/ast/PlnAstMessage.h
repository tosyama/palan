/// Message definitions.
///
/// Manage output message descriptions in Palan parser.
///
/// @file	PlnAstMessage.h
/// @copyright	2021 YAMAGUCHI Toshinobu 

#include <string>
using std::string;

#define PAT_ERR_MSG_START_NO	0
enum PlnErrCode {
	E_CouldnotOpenFile = PAT_ERR_MSG_START_NO,	// file name
};

enum PlnHelpCode {
	H_Help,
	H_Output,
	H_Indent,
	H_Input
};

class PlnAstMessage
{
public:
	static string getErr(PlnErrCode err_code, string arg1="\x01", string arg2="\x01");
	static const char* getHelp(PlnHelpCode help_code);
};
