/// Codegen Message management definitions.
///
/// Manage output message descriptions.
///
/// @file PlnCodegenMessage.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnCodegenMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnCodegenMessage::getMessage(PlnCodegenMessageCode msg_code, string arg1)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-codegen [options] <SA file>\n"
				"       palan-codegen -e <symbol> [-e <symbol>]... -o <file>\n"
				"Generate x86-64 assembly from palan SA Json, or the program entry.\n"
				"\n"
				"Options\n"
				" -o, --output <file>   Output assembly file path.\n"
				" -e, --entry <symbol>  Emit the _start that calls each <symbol> in order (a\n"
				"                       module's ..init, then its ..main), instead of\n"
				"                       compiling an SA file.\n"
				" --target <name>       With -e, specify the target.\n"
				" -v, --version         Display version.\n"
				" -h, --help            Display help.\n";

		case M_Version:
			return "palan-codegen " PALAN_VERSION;

		case E_UnsupportedTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan-codegen: unsupported target '" + arg1 + "'";

		case E_UnknownType:
			BOOST_ASSERT(arg1 != "\x01");
			return "Unknown type in SA file: '" + arg1 + "'.";

		case E_UnknownExprType:
			BOOST_ASSERT(arg1 != "\x01");
			return "Unknown expression type in SA file: '" + arg1 + "'.";

		case E_UnknownStmtType:
			BOOST_ASSERT(arg1 != "\x01");
			return "Unknown statement type in SA file: '" + arg1 + "'.";

		default:
			BOOST_ASSERT(false);
	}
}
