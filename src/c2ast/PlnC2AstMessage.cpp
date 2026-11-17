/// C2Ast Message management definitions.
///
/// Manage output message descriptions.
///
/// @file PlnC2AstMessage.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnC2AstMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnC2AstMessage::getMessage(PlnC2AstMessageCode msg_code, string arg1)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-c2ast [options] <C header file>\n"
				"Translate C header to palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output <file>   Output AST to file.\n"
				" -i, --indent          Output AST json with indent.\n"
				" -p, --path <dir>      Add search path for C headers.\n"
				" -c, --curdir <dir>    Specify current directory for relative includes.\n"
				" -s, --sysheader       Treat input as a system header.\n"
				" -S, --state <file>    Start from an earlier run's \"c-state\" output\n"
				"                       instead of predefined.h.\n"
				" --target <name>       Specify the target.\n"
				" -v, --version         Display version.\n"
				" -h, --help            Display help.\n";

		case M_Version:
			return "palan-c2ast " PALAN_VERSION;

		case E_NoInputFile:
			return "palan-c2ast: no input file";

		case E_UnsupportedTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan-c2ast: unsupported target '" + arg1 + "'";

		case E_ErrorDirective:
			BOOST_ASSERT(arg1 != "\x01");
			return arg1;

		case E_InvalidDefinedSyntax:
			return "invalid syntax in 'defined'";

		case E_InvalidHasIncludeSyntax:
			return "'__has_include' expects (\"FILENAME\") or (<FILENAME>)";

		case E_MalformedMacroArgList:
			return "malformed macro argument list";

		case E_UnhandledToken:
			return "unexpected token";

		case E_UnsupportedAttribute:
			BOOST_ASSERT(arg1 != "\x01");
			return "attribute '" + arg1 + "' is not supported here";

		case W_ExtraTokensAfterDirective:
			BOOST_ASSERT(arg1 != "\x01");
			return "warning: extra tokens at end of '#" + arg1 + "' directive";

		case E_InvalidDirective:
			return "invalid preprocessing directive";

		case E_MacroNameNotIdentifier:
			return "macro name must be an identifier";

		case E_ElifWithoutIf:
			return "#elif without #if";

		case E_ElifAfterElse:
			return "#elif after #else";

		case E_ElseWithoutIf:
			return "#else without #if";

		case E_EndifWithoutIf:
			return "#endif without #if";

		case E_ElseAfterElse:
			return "#else after #else";

		case E_IncludeNotFound:
			BOOST_ASSERT(arg1 != "\x01");
			return arg1 + ": No such file or directory";

		case E_IncludeExpectsFilename:
			return "#include expects \"FILENAME\" or <FILENAME>";

		case E_IfExprUnexpectedEnd:
			return "unexpected end of expression";

		case E_IfExprExtraTokens:
			return "unexpected token in expression";

		case E_IfExprMissingColon:
			return "missing ':' in conditional expression";

		case E_IfExprMissingParen:
			return "missing ')' in expression";

		case E_IfExprInvalidToken:
			return "expression is not valid";

		case E_MacroPasteAtStart:
			return "'##' cannot appear at start of macro expansion";

		case E_MacroPasteAtEnd:
			return "'##' cannot appear at end of macro expansion";

		case E_MacroPasteInvalidToken:
			return "pasting does not give a valid token";

		case E_MacroStringifyInvalidToken:
			return "invalid token in stringification";

		default:
			BOOST_ASSERT(false);
	}
}
