/// SA Message Management.
///
/// Manage output message descriptions class.
///
/// @file PlnSaMessage.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
using std::string;

enum PlnSaMessageCode {
	M_Help,
	M_Version,
	E_DuplicateVarDecl,		// arg1: variable name
	E_DuplicateFuncDef,		// arg1: function name
	E_IncompatibleTypeCast,	// arg1: from type, arg2: to type
	E_UndefinedFunction,	// arg1: function name
	E_InvalidNarrowingConv,	// arg1: source type name, arg2: target type name
	E_ExportInBlock,		// arg1: function name
	E_ExportInFunction,		// arg1: function name
	E_UndefinedVariable,	// arg1: variable name
	E_ReturnOutsideFunction,
	E_NamedRetBareReturn,
	E_SingleRetOneExpr,
	E_VoidBareReturn,
	E_VoidCallUsedAsValue,
	E_TupleUndefinedFunction,	// arg1: function name
	E_TupleNeedsMultiRet,		// arg1: function name
	E_TupleVarCountMismatch,	// arg1: function name
	E_ImportFileNotFound,		// arg1: file path
	E_NoInputFile,
	E_InternalError,			// arg1: error code
	E_BreakOutsideLoop,
	E_ContinueOutsideLoop,
	E_FloatModulo,
	E_ArraySizeNotInteger,
	E_ArrayIndexNotInteger,
	E_NotArrayType,
	E_UnsizedArrVarDecl,
	E_LogicalOpNotInteger,
	E_EmbeddedArrUnsizedInner,		// []$[]T in parameter: inner dimension must be fixed
	E_EmbeddedArrInnerSizeMismatch,	// arg1: expected inner-size, arg2: actual
	E_AmbiguousCall,			// arg1: function name
	E_UnknownAlias,				// arg1: alias name
	E_UnqualifiedAliasCall,			// arg1: function name
	E_UnknownType,				// arg1: type name
	E_UnknownField,				// arg1: struct name, arg2: field name
	E_UnsupportedStructFieldType,
	E_RecursiveStruct,			// arg1: struct name
	E_FieldAccessOnNonStruct,
	E_WriteToImmutablePtrField,
	E_EmbedOwningType,			// arg1: type display name
	E_WriteToReadOnlyArrElem,
	E_ArrFieldSizeNotConstant,	// arg1 (optional): the name that is not a const
	E_ConflictingTypedef,			// arg1: typedef name
	E_ConstNotCompileTimeValue,		// arg1: const name
	E_AddrOfNotLocalVar,			// arg1: variable name
	E_AddrOfNotPrimitive,			// arg1: variable or field name
	E_WriteThroughReadOnlyPtr,
	E_PtrMutabilityUpgrade,
	E_AssignToWholeStructElem,
	E_AddrOfNotAddressable,
	E_ReadOnlyPtrToNonConstCParam,		// arg1: C function name, arg2: parameter name
	E_AddrOfNotPrimitiveElem,
	E_BitwiseOpNotInteger,
	E_IncompleteStructType,		// arg1: struct name, arg2: incomplete reason ("unsupported-field" or "forward-declared")
	E_UnsupportedParamType,		// arg1: function name, arg2: unsupported type display name
	E_UnsupportedCFuncSignature,	// arg1: C function name, arg2: unsupported type display name
	E_GlobalNotWritable,		// arg1: variable name
	E_GlobalNotReplaceable,		// arg1: variable name
	E_UnsupportedCGlobalType,		// arg1: C global variable name, arg2: unsupported type display name
	E_ArithOpNotNumeric,
	E_StructInitNotSupported,	// arg1: struct type name
	E_IncompatibleTypes,		// arg1: from type display name, arg2: to type display name
	E_FuncNotValue,		// arg1: function name, arg2: the reason
	E_FuncPtrMutable,
	E_FuncPtrSigType,	// arg1: type display name
	E_KeepOutsideCast,
	E_CallNonFuncPtr,	// arg1: type display name
	E_AmbiguousFuncPtrCall,	// arg1: the name called
	E_DerefVoidPointer,		// no args
	E_InvalidLinkLibName,		// arg1: the library name as written
	E_LinkObjNotFound,			// arg1: the object file's absolute path
	E_SyscallNumberNotConstant,	// arg1: syscall name
	E_SyscallNumberOutOfRange,	// arg1: syscall name, arg2: the number as written
	E_SyscallTooManyParams,		// arg1: syscall name, arg2: parameter count
	E_SyscallUnsupportedParamType,	// arg1: syscall name, arg2: unsupported type display name
	E_VarTypeInferenceNotImpl,	// arg1: variable name
	E_StmtNotImplemented,
	E_ExprNotImplemented,
	E_ParamNotImplemented,
	E_RetNotImplemented,
	E_IntLiteralOutOfRange,	// arg1: the literal as written, arg2: adopted type display name
	E_WideCharLitNarrowType,	// arg1: code point (U+XXXX), arg2: type display name
	E_ArrLitContext,
	E_ArrLitCountMismatch,	// arg1: variable name, arg2: declared size, arg3: literal element count
	E_ArrLitSizeNotConst,	// arg1: variable name
	E_ArrLitDimMismatch,	// arg1: variable name
	E_ArrLitElemType,	// arg1: variable name
	E_ArrLitRowSizeMismatch,	// arg1: variable name, arg2: row size, arg3: the row's element count
	E_ArrBorrowSizeNotConst,
	E_ArrBorrowUnsupportedElem,
	E_ArrBorrowNeedsAddrOf,
	E_ArrBorrowShapeMismatch,	// arg1: expected shape, arg2: actual shape
	E_StructBorrowNeedsAddrOf,
	E_StructLitFieldCount,	// arg1: struct name, arg2: field count, arg3: value count
	E_StructLitDupField,	// arg1: field name
	E_StructLitMissingField,	// arg1: struct name, arg2: field name
	E_StructLitExpected,	// arg1: struct name
	E_StructLitFieldType,	// arg1: struct name, arg2: field name
	E_DictLitContext,
	E_ArrLitArgType,	// arg1: parameter name, arg2: parameter type
	E_DictLitArgType,	// arg1: parameter name, arg2: parameter type
	E_LitArgCFunc,	// arg1: C function name
	E_CopyShapeMismatch,	// arg1: destination shape, arg2: source shape
	E_AddrOfBorrowed,	// arg1: variable name
	E_EmbedElemNotStruct,	// arg1: element type
	E_NamedRetInitOnStruct,	// arg1: return name
	E_TransferToNonOwnedField,	// arg1: field name
	E_ExpiringStructToBorrow,
	E_ConvertedLiteralToBorrow,	// arg1: type
	E_TransferFromNonOwner,	// arg1: operator
	E_TransferNotArg,
	E_TransferToParam,	// arg1: parameter, arg2: function
	E_TransferCast,
	E_MoveFromReadOnlyView,	// arg1: operator
	E_TakeIntoFrameVar,	// arg1: variable name
	E_TransferToBorrowSlot,
	E_TransferShapeMismatch,	// arg1: destination shape, arg2: source shape
	E_TransferToNonOwnedVar,	// arg1: variable name, arg2: operator
	E_TransferToSelf,	// arg1: variable name
	E_GivenChangedByLaterArg,	// arg1: variable name
	E_UnsizedArrRowSizeNotConst,
	E_ArrElemShapeMismatch,	// arg1: expected shape, arg2: actual shape
	E_ArgCountMismatch,	// arg1: function name, arg2: expected count, arg3: actual count
	E_UnknownEnumerator,	// arg1: enumerator name, arg2: enum type name
	E_UnqualifiedEnumerator,	// arg1: enumerator name, arg2: quoted `Name.X` candidates
	E_DuplicateEnumerator,	// arg1: enumerator name, arg2: enum type name
	E_EnumValueNotConst,	// arg1: enumerator name
	E_EnumValueOutOfRange,	// arg1: enumerator name, arg2: value
	E_DuplicateTypeName,	// arg1: type name
	E_DuplicateConstDecl,	// arg1: const name
	E_CastArgCount,	// arg1: target type name
	E_SizeofArrSizeNotConst,
	E_MethodNeedsMutableFirstParam,	// arg1: function name
	E_UnqualifiedAliasType,	// arg1: type name, arg2: module alias
	E_CircularTypeDecl,	// arg1: type name, arg2: dependency path
	E_InvalidCastTarget,	// arg1: target type name
	E_OwnerViewNeedsPtr,	// arg1: target type name
	E_OwnerViewTargetNeedsPtr,	// arg1: target type name
	E_KeepWithoutSource,	// arg1: source type name
	E_KeepParamCount,	// arg1: cast's parameter count, arg2: source type name, arg3: its parameter count
	E_FuncCastOwnership,	// arg1: source type name, arg2: target type name
	E_StructLitTarget,
	E_ArrLitTarget,	// arg1: variable name
	E_BorrowOfTempEscapes,
	E_UnownedCallResult,
	E_AssignInExpr,
	E_ChainMultiToTapple,
	E_UnsupportedCFieldType,	// arg1: struct name, arg2: field name, arg3: unsupported type display name
	E_PtrInnerPermissionNarrowing,	// arg1: from type display name, arg2: to type display name
	E_ConstExprOverflow,
	E_ConstExprDivByZero,
	E_CMacroFuncLike,	// arg1: macro name
	E_CMacroNotConstant,	// arg1: macro name
	E_FrameVarType,	// arg1: type display name
	E_FrameArrSizeNotConst,	// arg1: variable name
	E_ReturnFrameArr,	// arg1: variable name
	E_NoMatchingFunc,	// arg1: function name, arg2: argument types, arg3: candidates
	E_FuncNameNeedsType,	// arg1: function name, arg2: candidates
	E_VarrBorrowNeedsAddrOf,
	E_ArrLenNotConst,
	E_ArrLenNeedsAddrOf,
	E_LenNotArray,	// arg1: the value's type display name
	E_VarrBorrowSized,
	E_ArrowBindsTighter,	// arg1: operator
	E_VarrElemSized,
	E_UnsizedVarrElemArr,
	E_ArrLitEmptySize,	// arg1: variable name
	E_SigArrSizeNotConst,
	E_ForRangeNotArray,
	E_AssignToForIndex,	// arg1: variable name
	E_ChangeForRange,	// arg1: the array iterated over
	E_ForIndexNotInteger,	// arg1: type display name
	E_ForIndexTooNarrow,	// arg1: type display name, arg2: the array's length
	E_ConstUsedBeforeKnown,	// arg1: const name, arg2: the name its value waits for
	E_GlobalNotTopLevel,
	E_GlobalPermitUnknownVar,	// arg1: variable name
	E_GlobalPermitNotFunc,	// arg1: function name
	E_GlobalPermitDuplicate,	// arg1: variable name, arg2: function name
	E_GlobalInitUsesTopVar,	// arg1: variable name, arg2: global name
	E_ExportedGlobalSizeNotConst,	// arg1: variable name
	E_InterfaceAsProgram,	// arg1: file path
	E_OperatorFuncArity,	// arg1: operator, arg2: parameter counts it takes
	E_OperatorFuncNoStruct,	// arg1: operator
	E_OperatorFuncWritableParam,	// arg1: operator, arg2: parameter name
	E_NoOperatorFunc,	// arg1: operator, arg2: operand types
	E_CompareFuncRet,	// arg1: operator
	E_FieldInitNotConst,	// arg1: field name
	E_FieldInitUnsupported,	// arg1: field name
	E_ParamDefaultNotConst,	// arg1: parameter name
	E_ParamDefaultNotLast,	// arg1: parameter name
	E_OperatorFuncDefault,	// arg1: operator, arg2: parameter name
	E_VariadicElemType,
	E_VariadicWithDefault,	// arg1: function name
	E_OperatorFuncVariadic,	// arg1: operator
	E_SyscallVariadic,	// arg1: syscall name
	E_VariadicArgTempBorrow,	// arg1: function name
	E_AppendFuncFirstParam,
	E_AppendFuncRet,	// arg1: type of the first parameter
	E_AppendNotStruct,	// arg1: left operand type
	E_AppendReadOnly,
	E_AppendLeftOperand,
	E_CopyFuncSig,
	E_CopyFuncPlace,
	E_CopyFuncExported,
	E_ForIterShape,		// arg1: struct type display name
	E_ForIterAmbiguous,	// arg1: struct type display name
	E_ForNextShape,		// arg1: iterator type display name
	E_ForElemForm,		// arg1: the loop variable as written ('@c' or '@!c'), arg2: next's element type
	E_FieldAccessNotExported,	// arg1: "private" or "readonly", arg2: field name
	E_PrivateField,		// arg1: struct type display name, arg2: field name
	E_ReadonlyField,	// arg1: struct type display name, arg2: field name
	E_StructLitHiddenField,	// arg1: struct type display name, arg2: field name, arg3: "private" or "readonly"
	E_StructLitPositionalHidden,	// arg1: struct type display name
	E_StructLitHiddenNoInit,	// arg1: struct type display name, arg2: field name, arg3: "private" or "readonly"
	E_OpaqueContents,	// arg1: contents type display name
	E_OpaqueNeedsLayout,	// arg1: opaque type display name, arg2: what needs the layout
	E_OpaqueField,		// arg1: opaque type display name
	E_OpaqueConvert,	// arg1: opaque type display name
	E_OpaqueConvertMove,
	E_OpaqueHeldCopy,	// arg1: the copied type's display name
	E_ReleaseFuncSig,
	E_ReleaseFuncPlace,
	E_ReleaseFuncDup,	// arg1: opaque type display name
	E_MoveRunSize,		// arg1: destination shape, arg2: source shape
	E_ResizeNotMoved,	// arg1: target type name
	E_UseAfterGiven,	// arg1: variable name
};

class PlnSaMessage
{
public:
	static string getMessage(PlnSaMessageCode msg_code,
	                         string arg1="\x01", string arg2="\x01",
	                         string arg3="\x01");
	// The program-wide name of a Palan struct/enum; seq is a block's order, 0
	// at the top level. A message shows it as written.
	static string typeIdentity(const string& moduleId, int seq, const string& name);
	static string writtenTypeName(const string& typeName);
private:
	static string format(PlnSaMessageCode msg_code, string arg1, string arg2, string arg3);
};
