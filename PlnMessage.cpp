#include "PlnMessage.h"
#include "boost/format.hpp"

using boost::format;

string PlnMessage::getErr(PlnErrCode err_code, string arg1, string arg2)
{
	string f;
	switch (err_code) {
		case E_UndefinedVariable:
			f  = "variable '%1%' was not declared in this scope."; break;
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
