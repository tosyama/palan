/// Build Manager Message management definitions.
///
/// Manage output message descriptions.
///
/// @file PlnBuildMgrMessage.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnBuildMgrMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnBuildMgrMessage::getMessage(PlnBuildMgrMessageCode msg_code, string arg1, string arg2, string arg3)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan [options] <source.pa> [program arguments...]\n"
				"Compile and run a Palan source file.\n"
				"\n"
				"Options\n"
				" -o, --output=<file>   Write binary to file (do not auto-run).\n"
				"                       With --lib, the output directory.\n"
				" --lib                 Build the library the source file is the main source of.\n"
				" --target=<name>       Specify the target. Only " PALAN_TARGET " is supported.\n"
				" -c, --clean           Remove all cached work files.\n"
				" -v, --version         Display version.\n"
				" -h, --help            Display help.\n";

		case M_Version:
			return "palan " PALAN_VERSION;

		case E_WrongNumberOfArgs:
			return "palan: wrong number of arguments";

		case E_CouldNotOpenFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: could not open file '" + arg1 + "'";

		case E_PalanDirNotDirectory:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: '" + arg1 + "' exists but is not a directory";

		case E_FailedToExecute:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "palan: failed to execute '" + arg1 + "': " + arg2;

		case E_UnsupportedTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: unsupported target '" + arg1 + "'";

		case E_DuplicateModuleId:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01" && arg3 != "\x01");
			return "palan: '" + arg1 + "' has the same module identity '" + arg2 + "' as '" + arg3 + "'";

		case E_LibNeedsOutput:
			return "palan: --lib needs an output directory (-o)";

		case E_LibModuleNotDeclared:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: '" + arg1 + "' has no library declaration, which every module of a library needs";

		case E_LibPackageMismatch:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01" && arg3 != "\x01");
			return "palan: '" + arg1 + "' belongs to library '" + arg2 + "', not '" + arg3 + "'";

		case E_LibModuleWithGlobals:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "palan: '" + arg1 + "' permits a function to use its global '" + arg2
				+ "', but a library's top level never runs";

		case E_LibSymbolMismatch:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "palan: the interface of '" + arg1 + "' declares '" + arg2
				+ "', which its object does not define";

		case E_LibraryNotFound:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "palan: '" + arg1 + "' imports <" + arg2 + ">, which no libpath has";

		case E_LibraryInTwoPlaces:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01" && arg3 != "\x01");
			return "palan: library '" + arg1 + "' is found in both '" + arg2 + "' and '" + arg3 + "'";

		case E_LibraryNoArchive:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: '" + arg1 + "' has no archive beside it";

		default:
			BOOST_ASSERT(false);
	}
}
