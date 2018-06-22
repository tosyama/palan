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
using std::cout;
using std::endl;
using std::unique_ptr;
using std::to_string;

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

struct PlnLoc {
	uint32_t begin_line;
	uint32_t end_line;
	uint16_t begin_col;
	uint16_t end_col;
	int16_t fid;
	PlnLoc() : fid(-1) {};
	string dump()
	{
		if (fid >= 0)
			return to_string(fid)
				+":"+to_string(begin_line)
				+"-"+to_string(end_line);
		return "none";
	};

};
