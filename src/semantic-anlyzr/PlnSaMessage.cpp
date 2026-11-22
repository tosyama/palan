/// SA Message management definitions.
///
/// Manage output message descriptions.
///
/// @file PlnSaMessage.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnSaMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>
#include <cctype>

using std::to_string;

static const char typePrefix[] = "__pln_L";

string PlnSaMessage::typeIdentity(const string& moduleId, int seq, const string& name)
{
	string module;
	for (char c : moduleId) module += c == '.' ? "_2e" : string(1, c);
	return typePrefix + to_string(module.size()) + module + "_" + (seq ? to_string(seq) + "_" : "") + name;
}

// Drops the module part of every type identity in `s`, whose length says
// where it ends. A seq is all digits, which no written name starts with.
static string dropTypeModules(const string& s)
{
	static const size_t prefixLen = sizeof typePrefix - 1;
	string out;
	size_t pos = 0;
	for (size_t at; (at = s.find(typePrefix, pos)) != string::npos; ) {
		out.append(s, pos, at - pos);
		size_t p = at + prefixLen, digits = p;
		while (digits < s.size() && isdigit((unsigned char)s[digits])) digits++;
		size_t end = digits == p ? string::npos : digits + stoul(s.substr(p, digits - p));
		if (end >= s.size() || s[end] != '_') { // LCOV_EXCL_BR_LINE
			out.append(typePrefix); // LCOV_EXCL_LINE
			pos = p; // LCOV_EXCL_LINE
			continue; // LCOV_EXCL_LINE
		}
		pos = end + 1;
		size_t seq = pos;
		while (seq < s.size() && isdigit((unsigned char)s[seq])) seq++;
		if (seq > pos && seq < s.size() && s[seq] == '_') pos = seq + 1;
	}
	out.append(s, pos, string::npos);
	return out;
}

string PlnSaMessage::writtenTypeName(const string& typeName)
{
	return dropTypeModules(typeName);
}

string PlnSaMessage::getMessage(PlnSaMessageCode msg_code, string arg1, string arg2, string arg3)
{
	return dropTypeModules(format(msg_code, arg1, arg2, arg3));
}

string PlnSaMessage::format(PlnSaMessageCode msg_code, string arg1, string arg2, string arg3)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-sa [options] <ast.json>\n"
				"Semantic analyze palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output=<output-path>   Output SA result to file.\n"
				" -i, --indent                 Output SA json with indent.\n"
				" --interface                  Check a .pai's AST; output its functions' symbols.\n"
				" --library=<pkg/mod>=<ast>    Where the AST of an import <pkg/mod> is.\n"
				" --release                    Leave out the run-time checks of a debug build.\n"
				" -v, --version                Display version.\n"
				" -h, --help                   Display help.\n";

		case M_Version:
			return "palan-sa " PALAN_VERSION;

		case E_DuplicateVarDecl:
			BOOST_ASSERT(arg1 != "\x01");
			return "Variable '" + arg1 + "' is already declared.";

		case E_DuplicateFuncDef:
			BOOST_ASSERT(arg1 != "\x01");
			return "Function '" + arg1 + "' is already defined.";

		case E_IncompatibleTypeCast:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Cannot cast type '" + arg1 + "' to '" + arg2 + "'.";

		case E_UndefinedFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined function '" + arg1 + "'.";

		case E_InvalidNarrowingConv:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Implicit conversion from '" + arg1 + "' to '" + arg2
				+ "' is not allowed; write an explicit cast '"
				+ arg2 + "(...)'.";

		case E_ExportInBlock:
			BOOST_ASSERT(arg1 != "\x01");
			return "Export is not allowed inside a block scope.";

		case E_ExportInFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Export is not allowed inside a function scope.";

		case E_UndefinedVariable:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined variable '" + arg1 + "'.";

		case E_ReturnOutsideFunction:
			return "Return statement outside of function.";

		case E_NamedRetBareReturn:
			return "Function with named return values requires bare 'return;' statement.";

		case E_SingleRetOneExpr:
			return "Single-return function requires exactly one return expression.";

		case E_VoidBareReturn:
			return "Void function requires bare 'return;' statement.";

		case E_VoidCallUsedAsValue:
			return "Void function call cannot be used as a value.";

		case E_TupleUndefinedFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined function '" + arg1 + "'.";

		case E_TupleNeedsMultiRet:
			BOOST_ASSERT(arg1 != "\x01");
			return "Function '" + arg1 + "' does not have multiple return values.";

		case E_TupleVarCountMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			return "Variable count does not match return count of '" + arg1 + "'.";

		case E_ImportFileNotFound:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open import file '" + arg1 + "'.";

		case E_NoInputFile:
			return "palan-sa: no input file";

		case E_InternalError:
			BOOST_ASSERT(arg1 != "\x01");
			return "Internal error: " + arg1;

		case E_BreakOutsideLoop:
			return "Break statement outside of loop.";

		case E_ContinueOutsideLoop:
			return "Continue statement outside of loop.";

		case E_FloatModulo:
			return "'%' operator is not supported for float types.";

		case E_ArraySizeNotInteger:
			return "Array size expression must be an integer type.";

		case E_ArrayIndexNotInteger:
			return "Array index expression must be an integer type.";

		case E_NotArrayType:
			return "Expression is not an array (pointer) type.";

		case E_UnsizedArrVarDecl:
			return "Unsized array type cannot be used in variable declaration.";

		case E_LogicalOpNotInteger:
			return "Logical operator operand must be an integer type.";

		case E_EmbeddedArrUnsizedInner:
			return "Embedded array parameter must have a fixed inner dimension (use []$[n]T).";

		case E_EmbeddedArrInnerSizeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Embedded array inner size mismatch: expected " + arg1 + ", got " + arg2 + ".";

		case E_AmbiguousCall:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Ambiguous function call '" + arg1 + "': it can mean " + arg2 + ".";

		case E_UnknownAlias:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined variable or module alias '" + arg1 + "'.";

		case E_UnqualifiedAliasCall:
			BOOST_ASSERT(arg1 != "\x01");
			return "Function '" + arg1 + "' requires a module alias qualifier.";

		case E_UnknownType:
			BOOST_ASSERT(arg1 != "\x01");
			return "unknown type '" + arg1 + "'.";

		case E_UnknownField:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 == "struct" || arg3 == "union");
			return arg3 + " '" + arg1 + "' has no field '" + arg2 + "'.";

		case E_UnsupportedStructFieldType:
			return "unsupported struct field type.";

		case E_RecursiveStruct:
			BOOST_ASSERT(arg1 != "\x01");
			return "struct '" + arg1 + "' recursively contains itself.";

		case E_FieldAccessOnNonStruct:
			return "field access on non-struct variable.";

		case E_WriteToImmutablePtrField:
			return "cannot write through read-only pointer field '@T'; use '@!T' for mutable.";

		case E_EmbedOwningType:
			return "cannot embed '" + arg1 + "' with '$': it owns storage of its own; use '" + arg1 + "' without '$'.";

		case E_WriteToReadOnlyArrElem:
			return "cannot write through read-only pointer array element; use [n]@!T for mutable.";

		case E_ArrFieldSizeNotConstant:
			if (arg1 != "\x01")
				return "struct field array size must be a compile-time constant integer; '" + arg1
				       + "' is not a const.";
			return "struct field array size must be a compile-time constant integer.";

		case E_ConflictingTypedef:
			BOOST_ASSERT(arg1 != "\x01");
			return "Typedef '" + arg1 + "' conflicts with an existing type alias of a different underlying type.";

		case E_ConstNotCompileTimeValue:
			BOOST_ASSERT(arg1 != "\x01");
			return "const '" + arg1 + "' must be initialized with a compile-time constant.";

		case E_AddrOfNotLocalVar:
			BOOST_ASSERT(arg1 != "\x01");
			return "'@'/'@!' address-of requires a local variable, not a parameter or '" + arg1 + "'.";

		case E_AddrOfNotPrimitive:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot take the address of '" + arg1 + "': '@'/'@!' only supports a primitive-typed "
			       "local variable, a primitive-typed pointer local, a struct or array variable, a "
			       "primitive-typed, embedded-struct or owned-struct field, or a primitive-typed or struct "
			       "array element of a struct/array it names.";

		case E_WriteThroughReadOnlyPtr:
			return "cannot write through read-only pointer '@T'; use '@!T' for mutable.";

		case E_PtrMutabilityUpgrade:
			return "cannot bind a read-only pointer '@T' to a mutable pointer '@!T'.";

		case E_AssignToWholeStructElem:
			return "cannot assign to a struct element as a whole; assign to its fields instead (e.g. 'p[i].field').";

		case E_AddrOfNotAddressable:
			return "cannot take the address of this expression: '@'/'@!' supports a local variable, a "
			       "primitive-typed or embedded-struct field of a struct it names, a primitive-typed "
			       "array element, or a call returning a struct or array given as a call argument.";

		case E_ReadOnlyPtrToNonConstCParam:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot pass read-only pointer '@T' to non-const parameter '" + arg2
			       + "' of C function '" + arg1 + "'; use '@!T' or pass a mutable variable.";

		case E_AddrOfNotPrimitiveElem:
			return "cannot take the address of this array element: '@'/'@!' only supports a "
			       "primitive-typed or struct array element, or a row (not, e.g., a pointer element).";

		case E_BitwiseOpNotInteger:
			return "Bitwise operator operand must be an integer type.";

		case E_IncompleteStructType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 == "unsupported-field" || arg2 == "forward-declared");
			BOOST_ASSERT(arg3 == "struct" || arg3 == "union");
			if (arg2 == "forward-declared")
				return arg3 + " '" + arg1 + "' is only forward-declared in this header; it can "
				       "only be used through a pointer ('@" + arg1 + "' / '@!" + arg1 + "').";
			return arg3 + " '" + arg1 + "' has no known layout (a field type is not supported "
			       "this version); it can only be used through a pointer ('@" + arg1 + "' / '@!"
			       + arg1 + "').";

		case E_UnsupportedParamType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "function '" + arg1 + "' has a parameter or return type this version cannot "
			       "represent: '" + arg2 + "'.";

		case E_UnsupportedCFuncSignature:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot call C function '" + arg1 + "': its signature uses '" + arg2
			       + "', a C type this version cannot represent; only integer and floating-point "
			         "types, pointers, and struct pointers are supported in a cinclude'd signature.";

		case E_GlobalNotWritable:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot write to '" + arg1 + "': a global variable is written only where a 'global' "
			       "statement permits it ('global " + arg1 + " for f!;' for function f; for a C global at "
			       "the top level, 'global " + arg1 + ";').";

		case E_UnsupportedCGlobalType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot reference C global variable '" + arg1 + "': its type uses '" + arg2
			       + "', a C type this version cannot represent.";

		case E_ArithOpNotNumeric:
			return "Arithmetic operator operand must be a numeric type.";

		case E_StructInitNotSupported:
			BOOST_ASSERT(arg1 != "\x01");
			return "a '" + arg1 + "' variable can only be initialized from a call that returns a '"
			       + arg1 + "' (a Palan function, or a C function returning it by value).";

		case E_IncompatibleTypes:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot convert '" + arg1 + "' to '" + arg2 + "'.";

		case E_FuncNotValue:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "function '" + arg1 + "' cannot be used as a function pointer: " + arg2 + ".";

		case E_FuncPtrMutable:
			return "a function pointer is read-only; write '@func(...)', not '@!func(...)'.";

		case E_FuncPtrSigType:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' cannot be a parameter or return type of a function pointer; "
			       "use a primitive type, a pointer, or an owned struct or array ('T', '[n]T', '[+]T').";

		case E_CallNonFuncPtr:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is not a function pointer and cannot be called.";

		case E_KeepOutsideCast:
			return "'_' keeps a type only in a function type a function pointer is cast to.";

		case E_AmbiguousFuncPtrCall:
			BOOST_ASSERT(arg1 != "\x01");
			return "call to '" + arg1 + "' is ambiguous: it names both a function pointer and a "
			       "function.";

		case E_DerefVoidPointer:
			return "cannot index or dereference a void pointer; assign it to a typed pointer "
			       "(e.g. @!int8) first.";

		case E_InvalidLinkLibName:
			BOOST_ASSERT(arg1 != "\x01");
			return "invalid library name '" + arg1 + "' in a cinclude link clause; a name "
			       "may only contain letters, digits, '_', '.', '+' and '-', since it is "
			       "passed to the linker as -l<name>.";

		case E_LinkObjNotFound:
			BOOST_ASSERT(arg1 != "\x01");
			return "object file '" + arg1 + "' in a cinclude link clause is not found.";

		case E_SyscallNumberNotConstant:
			BOOST_ASSERT(arg1 != "\x01");
			return "syscall '" + arg1 + "' number must be a literal integer; a symbolic "
			       "constant (e.g. SYS_write) or any other expression is not allowed.";

		case E_SyscallNumberOutOfRange:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "syscall '" + arg1 + "' number '" + arg2 + "' does not fit in a 32-bit "
			       "unsigned integer.";

		case E_SyscallTooManyParams:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "syscall '" + arg1 + "' has " + arg2 + " parameters; the Linux syscall "
			       "ABI supports at most 6.";

		case E_SyscallUnsupportedParamType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "syscall '" + arg1 + "' has a parameter or return type this version "
			       "cannot pass through the Linux syscall ABI: '" + arg2 + "'.";

		case E_VarTypeInferenceNotImpl:
			BOOST_ASSERT(arg1 != "\x01");
			return "variable '" + arg1 + "' is declared without a type; type inference is not "
			       "supported yet, so write the type explicitly.";

		case E_StmtNotImplemented:
			return "this statement is not supported in this version.";

		case E_ExprNotImplemented:
			return "this expression is not supported in this version.";

		case E_ParamNotImplemented:
			return "this parameter is not supported in this version.";

		case E_RetNotImplemented:
			return "this return value is not supported in this version.";

		case E_IntLiteralOutOfRange:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Integer literal '" + arg1 + "' is out of range for type '" + arg2 + "'.";

		case E_WideCharLitNarrowType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "character literal " + arg1 + " needs an integer type of 32 bits or more, not '" + arg2
			       + "': no byte of UTF-8 text holds it.";

		case E_ArrLitContext:
			return "an array literal can only be used as an array or struct variable's initializer, "
			       "the source of '->' to a struct or an array, the value returned from a function returning a struct, "
			       "or an argument for a struct or borrowed array parameter, in this version.";

		case E_ArrLitCountMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "array '" + arg1 + "' has size " + arg2 + " but its array literal has " + arg3 + " elements.";

		case E_ArrLitSizeNotConst:
			BOOST_ASSERT(arg1 != "\x01");
			return "array '" + arg1 + "' given an array literal must have a compile-time constant size.";

		case E_ArrLitDimMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			return "the array literal's dimensions do not match array '" + arg1 + "'.";

		case E_ArrLitElemType:
			BOOST_ASSERT(arg1 != "\x01");
			return "array '" + arg1 + "' cannot be given an array literal: only numeric and struct element types are supported.";

		case E_ArrLitRowSizeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "array '" + arg1 + "' has rows of size " + arg2 + " but a row of its array literal has " + arg3 + " elements.";

		case E_ArrBorrowSizeNotConst:
			return "every size in a borrowed array type ('@[n]T'/'@![n]T') must be a compile-time constant.";

		case E_ArrBorrowUnsupportedElem:
			return "a borrowed array type ('@[n]T'/'@![n]T') supports only primitive, struct and '@T'/'@!T' pointer elements in this version.";

		case E_ArrBorrowNeedsAddrOf:
			return "an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as '@name' or '@!name' "
			       "('@[..]' or '@![..]' for a literal).";

		case E_ArrBorrowShapeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "array shape '" + arg2 + "' does not match the borrowed array type '" + arg1 + "'.";

		case E_StructBorrowNeedsAddrOf:
			return "a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' "
			       "('@f()' or '@!f()' for a call returning it, '@{..}', '@!{..}', '@[..]' or '@![..]' for a literal).";

		case E_VarrBorrowNeedsAddrOf:
			return "a variable-length array given to '@[+]T'/'@![+]T' must be written as '@name' or '@!name'.";

		case E_VarrBorrowSized:
			return "a borrowed variable-length array type is written '@[+]T' or '@![+]T', without a size.";

		case E_VarrElemSized:
			return "a variable-length array element is written '[+]T', without a size: it starts empty.";

		case E_UnsizedVarrElemArr:
			return "an unsized array of variable-length arrays ('[][+]T') is not supported in this version; borrow the array as '@[n][+]T' or '@[+][+]T'.";

		case E_ConstUsedBeforeKnown:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "const '" + arg1 + "' cannot be used here: its value uses '" + arg2
				+ "', which is not a top-level variable declared before the const.";

		case E_SigArrSizeNotConst:
			return "every size in an array parameter or return type must be a compile-time constant.";

		case E_ArrLitEmptySize:
			BOOST_ASSERT(arg1 != "\x01");
			return "array '" + arg1 + "' cannot take its size from an empty array literal '[]'.";

		case E_ArrowBindsTighter:
			BOOST_ASSERT(arg1 != "\x01");
			return "'->' binds tighter than '" + arg1 + "'; parenthesize the operation to store its result: "
			       "'(a " + arg1 + " b) -> c'.";

		case E_ArrLenNotConst:
			return "'len' cannot tell this array's length: an array declared with its size, or a variable-length array, keeps one, but a '[]T' does not.";

		case E_ForRangeNotArray:
			return "'for' takes an array, a dictionary, or a struct whose type has 'iter' and 'next', to iterate over.";

		case E_GlobalNotReplaceable:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot assign to '" + arg1 + "': a function writes the elements or fields of an array "
			       "or struct global variable, but does not replace it.";

		case E_GlobalNotTopLevel:
			return "'global' is allowed only at the top level of a module.";

		case E_GlobalPermitUnknownVar:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is not a global variable: declare it with 'global', import it, or cinclude the C global first.";

		case E_GlobalPermitNotFunc:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is not a function at the top level of this module.";

		case E_GlobalPermitDuplicate:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "'" + arg2 + "' is already permitted to use '" + arg1 + "'.";

		case E_GlobalInitUsesTopVar:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "the initializer of global '" + arg2 + "' cannot use top-level variable '" + arg1
			       + "': globals are initialized before the top level runs.";

		case E_ExportedGlobalSizeNotConst:
			BOOST_ASSERT(arg1 != "\x01");
			return "the sizes of exported global '" + arg1
			       + "' must be constants or its array literal's: an importer cannot know them otherwise.";

		case E_InterfaceAsProgram:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is an interface, which has no code to compile.";

		case E_AssignToForIndex:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot assign to '" + arg1 + "': the index or key of a 'for' loop is read-only.";

		case E_ChangeForRange:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot change what a 'for' loop iterates over ('" + arg1 + "').";

		case E_ForIndexNotInteger:
			BOOST_ASSERT(arg1 != "\x01");
			return "the index of a 'for' loop must be of an integer type, not '" + arg1 + "'.";

		case E_ForIndexTooNarrow:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "the index type '" + arg1 + "' cannot hold every index of an array of length " + arg2 + ".";

		case E_ArrLenNeedsAddrOf:
			return "an array given to 'len' must be written as '@name'.";

		case E_LenNotArray:
			BOOST_ASSERT(arg1 != "\x01");
			return "'len' takes an array or a dictionary; this value is '" + arg1 + "'.";

		case E_StructLitFieldCount:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "struct '" + arg1 + "' has " + arg2 + " fields but its literal has " + arg3 + " values.";

		case E_StructLitDupField:
			BOOST_ASSERT(arg1 != "\x01");
			return "field '" + arg1 + "' is given more than once in a struct literal.";

		case E_StructLitMissingField:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "the literal of struct '" + arg1 + "' is missing field '" + arg2 + "'.";

		case E_StructLitExpected:
			BOOST_ASSERT(arg1 != "\x01");
			return "a value of struct '" + arg1 + "' must be written as '[...]' or '{name: value, ...}'.";

		case E_StructLitStringKey:
			BOOST_ASSERT(arg1 != "\x01");
			return "a value of struct '" + arg1 + "' names its fields; a string key is for a dictionary.";

		case E_ForDictKeyTyped:
			return "the key of a 'for' loop over a dictionary is '@[]uint8', which is not written.";

		case E_DictBorrowNeedsAddrOf:
			return "a dictionary given to '@[\"\"]T'/'@![\"\"]T' must be written as '@name' or '@!name'.";

		case E_DictLitContext:
			return "a '{name: value}' literal can only be used as a struct value: a struct variable's "
			       "initializer, the source of '->' to a struct, an element of an array literal, or an "
			       "argument for a struct parameter.";

		case E_ArrLitArgType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "parameter '" + arg1 + "' of type '" + arg2 + "' cannot take an array literal: "
			       "only a struct or fixed-size array parameter can.";

		case E_DictLitArgType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "parameter '" + arg1 + "' of type '" + arg2 + "' cannot take a '{name: value}' literal: "
			       "only a struct parameter can.";

		case E_CMacroFuncLike:
			BOOST_ASSERT(arg1 != "\x01");
			if (arg2 == "\x01")
				return "C macro '" + arg1 + "' has a body Palan cannot translate.";
			return "C macro '" + arg1 + "' uses " + arg2 + ", which Palan cannot translate.";

		case E_CMacroUnresolvedName:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "C macro '" + arg1 + "' uses '" + arg2 + "', which does not name a C function or C variable here.";

		case E_CMacroNotConstant:
			BOOST_ASSERT(arg1 != "\x01");
			return "C macro '" + arg1 + "' cannot be used: it does not expand to an integer constant.";

		case E_LitArgCFunc:
			BOOST_ASSERT(arg1 != "\x01");
			return "a literal cannot be passed to C function '" + arg1 + "': "
			       "only a by-value struct parameter takes one.";

		case E_StructLitTarget:
			return "a struct literal cannot be given to a borrow '@T'; write to its '[0]' to copy "
			       "into the struct it points to.";

		case E_ArrLitTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "an array literal cannot be given to the borrowed array '" + arg1 + "'; '->' to a borrow "
			       "rebinds it, so write to its elements instead.";

		case E_CopyShapeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot copy '" + arg2 + "' into '" + arg1 + "': both need the same element type and layout, and sizes, the same where both are known at compile time.";

		case E_AddrOfBorrowed:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is already a borrow ('@T'/'@[n]T'/'@[+]T'); pass it by name.";

		case E_EmbedElemNotStruct:
			BOOST_ASSERT(arg1 != "\x01");
			return "'$' applies only to a struct element or a '$[m]' row, not '" + arg1 + "'.";

		case E_NamedRetInitOnStruct:
			BOOST_ASSERT(arg1 != "\x01");
			return "named return '" + arg1 + "' of a struct or array type cannot have an initializer.";

		case E_TransferToNonOwnedField:
			BOOST_ASSERT(arg1 != "\x01");
			return "'->>' needs a field that owns its value (T or [n]T); '" + arg1 + "' does not.";

		case E_ExpiringStructToBorrow:
			return "a struct or array returned by a call has no owner to borrow from; receive "
			       "it into a variable with '->' and borrow it with '@name' or '@!name' (only a call "
			       "argument can borrow it directly, as '@f()' or '@!f()').";

		case E_ConvertedLiteralToBorrow:
			BOOST_ASSERT(arg1 != "\x01");
			return "a literal converted to '" + arg1 + "' is a temporary value with no owner to "
			       "borrow from; declare a '" + arg1 + "' variable initialized with it and borrow that "
			       "(only a call argument or an operator's operand can borrow it directly).";

		case E_BorrowOfTempEscapes:
			return "a borrow returned by a call given a temporary struct may point to that struct, "
			       "which is freed before the statement ends; receive the struct into a variable "
			       "with '->' and pass it as '@name' or '@!name'.";

		case E_UnownedCallResult:
			return "a struct or array returned by a call has no owner to access it through; "
			       "receive it into a variable with '->' first.";

		case E_AssignInExpr:
			return "an assignment cannot be used inside an expression; write it as its own statement.";

		case E_ChainMultiToTapple:
			return "the return values of a '->' chain whose value is evaluated first (a multiple-return call, "
			       "or a value given to a method-form call where it or the receiver has a call) cannot be received with '(..)' "
			       "in this version.";

		case E_TransferFromNonOwner:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' needs a source that owns its value: an owning variable, an owned field "
			       "or element, a call result, or a pointer viewed as an owner ('p as T').";

		case E_TransferNotArg:
			return "'>>' gives a value away only as a call argument, to a parameter that owns it.";

		case E_TransferToParam:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "parameter '" + arg1 + "' of '" + arg2 + "' cannot take a value given away with '>>': "
			       "only a parameter that owns it can. A C function takes it when called through a "
			       "function pointer type owning that parameter: '(f as @func(T))(x>>)'.";

		case E_TransferCast:
			return "a value given away with '>>' cannot be cast: it is given as the type it has. "
			       "To give it to a C function, cast the function instead: '(f as @func(T))(x>>)'.";

		case E_MoveFromReadOnlyView:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' cannot move from a read-only pointer viewed as an owner ('@X as T'): "
			       "what it points to is not to be freed through it.";

		case E_TakeIntoFrameVar:
			BOOST_ASSERT(arg1 != "\x01");
			return "'<<=' needs a variable that owns its storage; '" + arg1 + "' is laid out in place.";

		case E_TransferToBorrowSlot:
			return "'->>' cannot give ownership to a borrowed pointer slot ('@T'/'@!T'); store into it with '->'.";

		case E_TransferShapeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot transfer '" + arg2 + "' into '" + arg1 + "': both need the same element type and layout, and the same sizes where both are known at compile time.";

		case E_TransferToNonOwnedVar:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "'" + arg2 + "' needs a variable that owns its array or struct; '" + arg1 + "' does not.";

		case E_GivenChangedByLaterArg:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is given away with '>>' and set to NULL just before the call, "
			       "but this later argument of the same call may change it.";

		case E_UseAfterGiven:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' was given away and is NULL here; fill it with '->>' before using it.";

		case E_CExportFunc:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "function '" + arg1 + "' cannot be exported to C: " + arg2 + ".";

		case E_SpreadPlace:
			return "'...' spreads an array only as the last argument of a call.";

		case E_SpreadNotVariadic:
			BOOST_ASSERT(arg1 != "\x01");
			return "'...' spreads an array only into a variadic parameter '[...]T', which '" + arg1 + "' does not have.";

		case E_SpreadPosition:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "the spread array gives all of '" + arg1 + "' of '" + arg2
			     + "': it must be the only argument after the other parameters'.";

		case E_SpreadNotArray:
			BOOST_ASSERT(arg1 != "\x01");
			return "'...' spreads an array, but this is " + arg1 + ".";

		case E_SpreadBorrow:
			return "'...' takes the array as a by-value parameter does: write '...a' for a copy or '...a>>' to give it, not '@a'.";

		case E_SpreadUnsized:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' keeps no length, so it cannot be spread with '...'.";

		case E_SpreadElemMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "'...' cannot spread '" + arg2 + "' into '" + arg1
			     + "': the elements must be of exactly its type, since they are not converted.";

		case E_AddrOfBitField:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot take the address of bit-field '" + arg1 + "'.";

		case E_BitFieldLiteralOutOfRange:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "Integer literal '" + arg1 + "' does not fit the " + arg3 + "-bit bit-field '" + arg2 + "'.";

		case E_TransferToSelf:
			BOOST_ASSERT(arg1 != "\x01");
			return "'->>' cannot give '" + arg1 + "' to itself.";

		case E_UnsizedArrRowSizeNotConst:
			return "the row size in '[][m]T' or '[+][m]T' must be a compile-time constant.";

		case E_ArrElemShapeMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "array shape '" + arg2 + "' does not match '" + arg1 + "': its elements must have the same size and layout, and be owned by both or by neither.";

		case E_ArgCountMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "function '" + arg1 + "' takes " + arg2 + " argument(s), but " + arg3 + " given.";

		case E_UnknownEnumerator:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "enum '" + arg2 + "' has no enumerator '" + arg1 + "'.";

		case E_UnqualifiedEnumerator:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Undefined variable '" + arg1 + "'; write " + arg2 + " for the enumerator.";

		case E_DuplicateEnumerator:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "enumerator '" + arg1 + "' is already defined in enum '" + arg2 + "'.";

		case E_EnumValueNotConst:
			BOOST_ASSERT(arg1 != "\x01");
			return "enumerator '" + arg1 + "' must be given an integer constant expression.";

		case E_EnumValueOutOfRange:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "value " + arg2 + " of enumerator '" + arg1 + "' is out of the int64 range.";

		case E_DuplicateTypeName:
			BOOST_ASSERT(arg1 != "\x01");
			return "type '" + arg1 + "' is already defined.";

		case E_DuplicateConstDecl:
			BOOST_ASSERT(arg1 != "\x01");
			return "Constant '" + arg1 + "' is already defined.";

		case E_CastArgCount:
			BOOST_ASSERT(arg1 != "\x01");
			return "cast '" + arg1 + "(...)' takes exactly one argument.";

		case E_SizeofArrSizeNotConst:
			return "every array size in the type of 'sizeof' must be a compile-time constant.";

		case E_MethodNeedsMutableFirstParam:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' cannot be called as a method: its first parameter must be a writable borrow ('@!').";

		case E_UnqualifiedAliasType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "type '" + arg1 + "' requires a module alias qualifier: '" + arg2 + "." + arg1 + "'.";

		case E_CircularTypeDecl:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "type '" + arg1 + "' depends on itself: " + arg2 + ".";

		case E_OwnerViewNeedsPtr:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot cast to '" + arg1 + "': only a pointer ('@!X') can be viewed as an owner, "
			       "and a cast never creates ownership of a value.";

		case E_OwnerViewTargetNeedsPtr:
			BOOST_ASSERT(arg1 != "\x01");
			return "'->>' cannot give a value to '" + arg1 + "': '->> loc as T' needs a location holding a "
			       "writable pointer ('@!X') and an owned type T (a struct, '[n]T' or '[+]T').";

		case E_KeepWithoutSource:
			BOOST_ASSERT(arg1 != "\x01");
			return "'_' keeps the type of the cast's source at its place, but '" + arg1 + "' has no such place.";

		case E_KeepParamCount:
			BOOST_ASSERT(arg3 != "\x01");
			return "the cast's function type has " + arg1 + (arg1 == "1" ? " parameter" : " parameters")
			       + ", but '" + arg2 + "' has " + arg3 + ".";

		case E_FuncCastOwnership:
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot cast '" + arg1 + "' to '" + arg2 + "': only a C function can be cast to take or give ownership.";

		case E_InvalidCastTarget:
			BOOST_ASSERT(arg1 != "\x01");
			return "cannot cast to '" + arg1 + "': only a primitive, enum, pointer, borrowed array ('@[n]T'/'@![n]T'), "
			       "or owned struct or array viewed from a pointer can be a cast target.";

		case E_UnsupportedCFieldType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "cannot access field '" + arg2 + "' of '" + arg1 + "': its type uses '" + arg3
			       + "', a C type this version cannot represent.";

		case E_PtrInnerPermissionNarrowing:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot bind '" + arg1 + "' to '" + arg2
			       + "': a pointer reached through a mutable pointer '@!' must keep its permission.";

		case E_ConstExprOverflow:
			return "constant expression overflows its 64-bit range.";

		case E_ConstExprDivByZero:
			return "division by zero in a constant expression.";

		case E_FrameVarType:
			return "'$' on a variable applies only to a struct or an array, not '" + arg1 + "'.";

		case E_FrameArrSizeNotConst:
			return "the size of stack array '" + arg1 + "' must be a compile-time constant.";

		case E_ReturnFrameArr:
			return "stack array '" + arg1 + "' cannot be returned; its storage ends with the function.";

		case E_NoMatchingFunc:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			BOOST_ASSERT(arg3 != "\x01");
			return "No function '" + arg1 + "' takes (" + arg2 + "); there are " + arg3 + ".";

		case E_OperatorFuncArity:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "operator function '" + arg1 + "' must take " + arg2 + ".";

		case E_OperatorFuncNoStruct:
			BOOST_ASSERT(arg1 != "\x01");
			return "operator function '" + arg1 + "' must take a struct ('T' or '@T') parameter.";

		case E_OperatorFuncWritableParam:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "parameter '" + arg2 + "' of operator function '" + arg1
			       + "' cannot be '@!': an operand is never passed for writing.";

		case E_CompareFuncRet:
			BOOST_ASSERT(arg1 != "\x01");
			return "operator function '" + arg1 + "' must return 'bool'.";

		case E_NoOperatorFunc:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "No operator function '" + arg1 + "' takes (" + arg2 + ").";

		case E_FuncNameNeedsType:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "'" + arg1 + "' names " + arg2 + "; it must be given where a function pointer type chooses one.";

		case E_FieldInitNotConst:
			BOOST_ASSERT(arg1 != "\x01");
			return "the initializer of field '" + arg1 + "' must be a compile-time constant.";

		case E_FieldInitPtrNotNull:
			BOOST_ASSERT(arg1 != "\x01");
			return "the initializer of pointer field '" + arg1 + "' must be NULL.";

		case E_ParamDefaultNotConst:
			BOOST_ASSERT(arg1 != "\x01");
			return "the default value of parameter '" + arg1 + "' must be a compile-time constant: a number, "
			       "'true' or 'false', an enum value, a const, or a string literal.";

		case E_ParamDefaultNotLast:
			BOOST_ASSERT(arg1 != "\x01");
			return "parameter '" + arg1 + "' needs a default value: it follows a parameter that has one, "
			       "and parameters with default values come last.";

		case E_OperatorFuncDefault:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "parameter '" + arg2 + "' of operator function '" + arg1
			       + "' cannot have a default value: an operator is always given its operands.";

		case E_VariadicElemType:
			return "a variadic parameter's elements cannot be an array by value ('[n]T', '[]T', '[+]T') or "
			       "'$T': each argument is given as a parameter is, so an array must be borrowed ('@[]T').";

		case E_VariadicWithDefault:
			BOOST_ASSERT(arg1 != "\x01");
			return "function '" + arg1 + "' cannot have both default values and a variadic parameter: "
			       "an argument could be taken for either.";

		case E_OperatorFuncVariadic:
			BOOST_ASSERT(arg1 != "\x01");
			return "operator function '" + arg1 + "' cannot take a variadic parameter: an operator has fixed operands.";

		case E_SyscallVariadic:
			BOOST_ASSERT(arg1 != "\x01");
			return "syscall '" + arg1 + "' cannot take a variadic parameter.";

		case E_AppendFuncFirstParam:
			return "the first parameter of operator function '<<' must be a writable borrow '@!T' of a struct.";

		case E_AppendFuncRet:
			BOOST_ASSERT(arg1 != "\x01");
			return "operator function '<<' must return '" + arg1 + "', the type of its first parameter, "
			       "so that '<<' chains.";

		case E_AppendNotStruct:
			BOOST_ASSERT(arg1 != "\x01");
			return "'<<' appends to a struct, not to '" + arg1 + "'; the left shift is '<<<'.";

		case E_AppendReadOnly:
			return "'<<' cannot append through a read-only borrow '@T'.";

		case E_AppendLeftOperand:
			return "the left operand of '<<' must be a variable, a field or an element, a '@!' borrow, "
			       "or a call returning '@!T': a value a call or an operator returns is a temporary, "
			       "and what is appended to it would be lost.";

		case E_CopyFuncSig:
			return "'->' must be 'func ->(@T src, @!T dst)' of one struct T, with no defaults and no return value.";

		case E_CopyFuncPlace:
			return "'->' is part of its type's definition: it is defined at the top level of the module "
			       "defining the struct, not of a C struct or of another module's.";

		case E_CopyFuncExported:
			return "'->' is not exported: it comes with its type wherever the type goes.";

		case E_ForIterShape:
			BOOST_ASSERT(arg1 != "\x01");
			return "'for' over '" + arg1 + "' needs 'func iter(@" + arg1 + " s) -> I' or 'func iter(@!" + arg1
			       + " s) -> I', I a struct type, and 'func next(@!I it) -> bool ok, E'.";

		case E_ForIterAmbiguous:
			BOOST_ASSERT(arg1 != "\x01");
			return "'for' over '" + arg1 + "' finds both 'iter(@" + arg1 + ")' and 'iter(@!" + arg1
			       + ")'; a type has one of them.";

		case E_ForNextShape:
			BOOST_ASSERT(arg1 != "\x01");
			return "'for' needs 'func next(@!" + arg1 + " it) -> bool ok, E' for the iterator '" + arg1 + "'.";

		case E_ForElemForm:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "'" + arg1 + "' needs 'next' to return the element as a borrow"
			       + string(arg1.rfind("@!", 0) == 0 ? " '@!E'" : " '@E' or '@!E'")
			       + ", but it returns '" + arg2 + "'.";

		case E_FieldAccessNotExported:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "'" + arg1 + "' on the field '" + arg2 + "' needs an exported struct ('export type'): "
			       "the boundary is the module defining it.";

		case E_PrivateField:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "the field '" + arg2 + "' of '" + arg1 + "' is private to the module defining '" + arg1 + "'.";

		case E_ReadonlyField:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return "the field '" + arg2 + "' of '" + arg1 + "' is read-only outside the module defining '" + arg1
			       + "', and so is anything reached through it.";

		case E_StructLitHiddenField:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01" && arg3 != "\x01");
			return "a struct literal of '" + arg1 + "' outside the module defining it cannot set the " + arg3
			       + " field '" + arg2 + "'.";

		case E_StructLitPositionalHidden:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' has private or read-only fields: outside the module defining it, a struct "
			       "literal of it names the fields it sets.";

		case E_StructLitHiddenNoInit:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01" && arg3 != "\x01");
			return "no struct literal of '" + arg1 + "' can be written outside the module defining it: its " + arg3
			       + " field '" + arg2 + "' has no initializer.";

		case E_OpaqueContents:
			BOOST_ASSERT(arg1 != "\x01");
			return "an opaque type holds what a variable owns through one pointer (a struct, an array of "
			       "constant sizes or a '[+]T'), not '" + arg1 + "'.";

		case E_OpaqueNeedsLayout:
			BOOST_ASSERT(arg1 != "\x01" && arg2 != "\x01");
			return arg2 + " needs the contents of the opaque type '" + arg1
			       + "', which only a module able to write their type knows.";

		case E_OpaqueField:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' is opaque and has no fields: its module reaches the contents through 'as'.";

		case E_OpaqueConvert:
			BOOST_ASSERT(arg1 != "\x01");
			return "an owned '" + arg1 + "' is converted only to or from its contents' type, in a module "
			       "able to write that type.";

		case E_OpaqueConvertMove:
			return "an owned value converted between an opaque type and its contents is given by a move: "
			       "'<<=', '->>', '>>' or 'return'.";

		case E_OpaqueHeldCopy:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' holds an opaque value, and copying one held in another value is not "
			       "supported in this version.";

		case E_ReleaseFuncSig:
			return "a 'release' function takes one owned opaque type of its module and returns nothing.";

		case E_ReleaseFuncPlace:
			return "a 'release' function is defined at the top level of the module defining its opaque type.";

		case E_ReleaseFuncDup:
			BOOST_ASSERT(arg1 != "\x01");
			return "'" + arg1 + "' has another 'release' function; a type has one.";

		case E_MoveRunSize:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "cannot transfer '" + arg2 + "' into '" + arg1 + "': its size cannot change there, and a size known "
			       "only at run time cannot be shown to match it; set the size with 'as' if they match.";

		case E_ResizeNotMoved:
			BOOST_ASSERT(arg1 != "\x01");
			return "an array cast to '" + arg1 + "' can only be given away: '<<=', '->>', '>>' or 'return'.";

		default:
			BOOST_ASSERT(false);
	}
}
