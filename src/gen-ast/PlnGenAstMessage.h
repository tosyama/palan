/// Message Management.
///
/// Manage output message descripitons class.
///
/// @file PlnGenAstMessage.h
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnMessageCode {
	M_Help,
	M_Version,
	E_CouldNotOpenFile,		// filename
	E_CouldNotOpenOutputFile,	// filename
	E_NoInputFile,
	E_UnsupportedTarget,		// target name
	E_C2AstFailed,			// header path
	E_ExpectedLinkKeyword,		// identifier
	E_UnexpectedChar,		// character
	E_InvalidCharLiteral,		// literal text
	E_InvalidNumberLiteral,		// literal text
	E_SyscallNamedReturn,
	E_ExpectedInKeyword,		// identifier
	E_ForElemNoMode,		// loop variable name
	E_ExpectedCopyKeyword,	// identifier
	E_InterfaceOnly,		// what was written
	E_InterfaceOtherVersion,	// the version it records
	E_InterfaceNoVersion,
	E_InterfaceOtherTarget,	// the target it records
	E_InterfaceNoTarget,
	E_NestedCIncludeExport,
	E_UnexpectedString,		// the identifier before it
	E_LibpathInInterface,
	E_BadLibraryImport,		// the path as written
	E_DerivedCompareFunc,	// operator, the operator it is derived from
	E_CharLitSeveralCodePoints,	// literal text
	E_CharLitInvalidUtf8,
	E_InvalidCodePointEscape,	// the escape as written
	E_VariadicNotLast,
	E_UnknownFieldMarker,	// the word before the field's type
};

class PlnGenAstMessage
{
public:
	static string getMessage(PlnMessageCode msg_code, string arg1="\x01", string arg2="\x01");
	static string locatedError(const string& file, int line, int column, const string& msg);
};


