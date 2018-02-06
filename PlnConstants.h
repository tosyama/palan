/// Difinition of constants to share.
///
/// @file	PlnConstants.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#pragma once

// Data type on stack/register.
enum {
	DT_SINT,	/// Signed integer.
	DT_UINT,	/// Unsigned integer.
	DT_FLOAT,	/// Floating-point number.
	DT_OBJECT,	/// Instance of object. (size <= 8byte)
	DT_OBJECT_REF,	/// Address of object.
	DT_UNKNOWN	/// Unkown data type. Not initialized.
};

// Function type.
enum {
	FT_PLN,		/// Palan standard function.
	FT_INLINE,	/// Palan inline function.
	FT_C,		/// C language function.
	FT_SYS,		/// System call.
	FT_UNKNOWN	/// Unkown function type. Not initialized.
};

// Comparison type.
enum PlnCmpType {
	CMP_NE
};

