/// Build model trees from AST json declaration.
///
/// @file	PlnModelTreeBuilder.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../libs/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

class PlnModule;
class PlnModelTreeBuilder
{
public:
	PlnModelTreeBuilder();
	PlnModule* buildModule(json& ast);
};
