/// C2Ast Message Management.
///
/// Manage output message descriptions class.
///
/// @file PlnC2AstMessage.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnC2AstMessageCode {
	M_Help,
	M_Version,
	E_NoInputFile,
	E_UnsupportedTarget,	// arg1: target name
	E_ErrorDirective,		// arg1: message text from #error directive
	E_InvalidDefinedSyntax,
	E_InvalidHasIncludeSyntax,
	E_MalformedMacroArgList,
	E_UnhandledToken,
	E_UnsupportedAttribute,	// arg1: attribute name
	W_ExtraTokensAfterDirective,	// arg1: directive name (undef/ifdef/else/endif)
	// Group A: directive errors
	E_InvalidDirective,
	E_MacroNameNotIdentifier,
	E_ElifWithoutIf,
	E_ElifAfterElse,
	E_ElseWithoutIf,
	E_EndifWithoutIf,
	E_ElseAfterElse,
	// Group B: #include errors
	E_IncludeNotFound,				// arg1: filename
	E_IncludeExpectsFilename,
	// Group C: #if expression errors
	E_IfExprUnexpectedEnd,
	E_IfExprExtraTokens,
	E_IfExprMissingColon,
	E_IfExprMissingParen,
	E_IfExprInvalidToken,
	// Group D: macro expansion errors
	E_MacroPasteAtStart,
	E_MacroPasteAtEnd,
	E_MacroPasteInvalidToken,
	E_MacroStringifyInvalidToken,
};

class PlnC2AstMessage
{
public:
	static string getMessage(PlnC2AstMessageCode msg_code, string arg1="\x01");
};
