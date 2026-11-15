/// Build Manager Message Management.
///
/// Manage output message descriptions class.
///
/// @file PlnBuildMgrMessage.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnBuildMgrMessageCode {
	M_Help,
	M_Version,
	E_WrongNumberOfArgs,
	E_CouldNotOpenFile,		// arg1: file path
	E_PalanDirNotDirectory,	// arg1: path string
	E_FailedToExecute,		// arg1: program name, arg2: strerror text
	E_UnsupportedTarget,	// arg1: target name
	E_DuplicateModuleId,	// arg1: source file, arg2: identity, arg3: other source file
	E_LibNeedsOutput,
	E_LibModuleNotDeclared,	// arg1: source file
	E_LibPackageMismatch,	// arg1: source file, arg2: its package, arg3: the main's package
	E_LibSymbolMismatch,	// arg1: main source file, arg2: symbol
	E_LibraryNotFound,		// arg1: importing file, arg2: module
	E_LibraryInTwoPlaces,	// arg1: package, arg2, arg3: directories
	E_LibraryNoArchive,		// arg1: .pai file
};

class PlnBuildMgrMessage
{
public:
	static string getMessage(PlnBuildMgrMessageCode msg_code,
	                         string arg1="\x01", string arg2="\x01", string arg3="\x01");
};
