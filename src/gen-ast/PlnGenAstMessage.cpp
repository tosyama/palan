/// Message management definisions.
///
/// Manage output message descripitons.
///
/// @file PlnGenAstMessage.cpp
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include "PlnGenAstMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnGenAstMessage::getMessage(PlnMessageCode msg_code, string arg1, string arg2)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-gen-ast [options] <path-to-source>...\n"
				"Generate palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output=<output-path>   Output AST to file.\n"
				" -i, --indent                 Output AST json with indent.\n"
				" --interface=<output-path>    Also output the module's declarations as an interface.\n"
				" --target=<name>              Specify the target.\n"
				" -v, --version                Display version.\n"
				" -h, --help                   Display help.\n";

		case M_Version:
			return "palan-gen-ast " PALAN_VERSION;

		case E_CouldNotOpenFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open file '" + arg1 + "'.";

		case E_CouldNotOpenOutputFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open output file '" + arg1 + "'.";

		case E_InterfaceOnly:
			BOOST_ASSERT(arg1 != "\x01");
			return arg1 + " is accepted only in an interface file (.pai).";

		case E_InterfaceOtherVersion:
			BOOST_ASSERT(arg1 != "\x01");
			return "this interface was built by version " + arg1 + ", not " PALAN_VERSION ".";

		case E_InterfaceNoVersion:
			return "an interface file needs a 'built' statement.";

		case E_InterfaceOtherTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "this interface is for the target " + arg1 + ", not the one being built for.";

		case E_InterfaceNoTarget:
			return "an interface file needs a 'target' statement.";

		case E_NestedCIncludeExport:
			return "an 'export' clause is accepted only on a top-level cinclude.";

		case E_UnexpectedString:
			BOOST_ASSERT(arg1 != "\x01");
			return "syntax error, unexpected string after '" + arg1 + "'.";

		case E_LibpathInInterface:
			return "an interface file cannot hold 'libpath'; the program's libpaths find what it imports.";

		case E_BadLibraryImport:
			BOOST_ASSERT(arg1 != "\x01");
			return "'<" + arg1 + ">' does not name a library module, which is written <package/module>, without '.'.";

		case E_NoInputFile:
			return "palan-gen-ast: no input file";

		case E_UnsupportedTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan-gen-ast: unsupported target '" + arg1 + "'";

		case E_C2AstFailed:
			BOOST_ASSERT(arg1 != "\x01");
			return "Failed to read C header '" + arg1 + "'.";

		case E_ExpectedLinkKeyword:
			BOOST_ASSERT(arg1 != "\x01");
			return "Expected 'link' keyword, but found '" + arg1 + "'.";

		case E_UnexpectedChar:
			BOOST_ASSERT(arg1 != "\x01");
			return "Unexpected character '" + arg1 + "'.";

		case E_InvalidCharLiteral:
			BOOST_ASSERT(arg1 != "\x01");
			return "Invalid character literal " + arg1 + ".";

		case E_InvalidNumberLiteral:
			BOOST_ASSERT(arg1 != "\x01");
			return "Invalid number literal '" + arg1 + "'.";

		case E_VariadicNotLast:
			return "a variadic parameter ('[...]T') must be the last parameter.";

		case E_UnknownFieldMarker:
			BOOST_ASSERT(arg1 != "\x01");
			return "Expected 'private' or 'readonly' before the field's type, but found '" + arg1 + "'.";

		case E_SyscallNamedReturn:
			return "a syscall must declare its return with '-> type'; named and multiple "
			       "return values are not supported.";

		case E_ExpectedInKeyword:
			BOOST_ASSERT(arg1 != "\x01");
			return "Expected 'in' after the loop variable, but found '" + arg1 + "'.";

		case E_ForElemNoMode:
			BOOST_ASSERT(arg1 != "\x01");
			return "The loop variable '" + arg1 + "' needs 'copy', '@' or '@!': 'copy " + arg1
			       + "' copies each element, '@" + arg1 + "'/'@!" + arg1 + "' is the element itself.";

		case E_ExpectedCopyKeyword:
			BOOST_ASSERT(arg1 != "\x01");
			return "Expected 'copy', '@' or '@!' before the loop variable, but found '" + arg1 + "'.";

		case E_CharLitSeveralCodePoints:
			BOOST_ASSERT(arg1 != "\x01");
			return "Character literal " + arg1 + " holds more than one code point; write it as a string literal.";

		case E_CharLitInvalidUtf8:
			return "Character literal is not valid UTF-8; write bytes in a string literal with '\\xHH'.";

		case E_InvalidCodePointEscape:
			BOOST_ASSERT(arg1 != "\x01");
			return "Invalid code point escape '" + arg1 + "': '\\u{X}' takes 1 to 6 hex digits, "
			       "up to 10FFFF and not a surrogate (D800 to DFFF).";

		case E_DerivedCompareFunc:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "'" + arg1 + "' cannot be defined: it is derived from '" + arg2 + "'.";

		default:
			BOOST_ASSERT(false);
	}
}

string PlnGenAstMessage::locatedError(const string& file, int line, int column, const string& msg)
{
	return file + ":" + std::to_string(line) + ":" + std::to_string(column) + ": error: " + msg;
}
