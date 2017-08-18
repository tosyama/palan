/// All model classes declaration.
///
/// @file	PlnModel.h
/// @copyright	2017- YAMAGUCHI Toshinobu 
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cstdint>

using std::string;
using std::vector;
using std::ostream;
using std::endl;

// parser
class PlnScopeItem;

// model
class PlnModule;
class PlnFunction;
class PlnBlock;
class PlnStatement;
	class PlnVarInit;
class PlnExpression;
	class PlnValue;
class PlnType;
class PlnVariable;
	class PlnParameter;

class PlnReadOnlyData;

// generator
class PlnGenerator;
class PlnGenEntity;


