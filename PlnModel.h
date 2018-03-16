/// All model classes declaration.
///
/// @file	PlnModel.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <memory>

using std::string;
using std::vector;
using std::ostream;
using std::endl;
using std::unique_ptr;

// parser
class PlnScopeItem;
class PlnScopeInfo;

// model
class PlnModule;
class PlnFunction;
class PlnBlock;
class PlnStatement;
	class PlnVarInit;
	class PlnReturnStmt;
class PlnExpression;
	class PlnValue;
class PlnType;
class PlnVariable;
	class PlnParameter;

class PlnReadOnlyData;
class PlnArrayItem;
class PlnDataAllocator;
class PlnDataPlace;

// generator
class PlnGenerator;
class PlnGenEntity;


