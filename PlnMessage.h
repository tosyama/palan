#include <string>
using std::string;

enum PlnErrCode {
	E_UndefinedVariable,
	E_UndefinedFunction,
	E_UndefinedType,
	E_DuplicateVarName,
	E_CouldnotOpenFile
};

enum PlnWarnCode {
};

class PlnMessage
{
public:
	static string getErr(PlnErrCode err_code, string arg1="", string arg2="");
	static string getWarn(PlnWarnCode warn_code, string arg1="", string arg2="");
};
