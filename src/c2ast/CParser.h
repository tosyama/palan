/// C parser: recursive descent parser for C declarations.
///
/// @file CParser.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
#include <map>
#include <set>
#include "../../lib/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

class CPreprocessor;
class CMacro;

class CParser {
	const vector<CToken*> &top_tokens;
	const vector<CLexer*> &lexers;
	map<string, json> typedefs_;  // typedef name -> resolved var-type (prim/pntr only, or a
	                               // tagged strct: {"type-kind":"strct","type-name":Tag})
	// The single registration point for typedefs_. Takes the var-type by value and
	// drops "typedef-name" -- that key is a reference-site annotation stamped on by
	// declaration_specifiers(), so without stripping it here, a chained typedef
	// (typedef A B;) would carry the previous link's name (A) into B's stored entry.
	void registerTypedef(const string &name, json vt);

	// Every struct/union tag seen while parsing (as a definition, a forward
	// declaration, or a bare reference through a field/parameter/return type),
	// plus any anonymous struct/union body given a synthesized tag via its typedef name (see
	// declaration()'s tag synthesis in CParser.cpp), keyed by tag name, in
	// first-appearance order. An entry gains a "fields" key only once a full
	// field-bearing definition of that tag is captured (see captureStructTag);
	// C struct and union tags share one namespace, so unions go into the same
	// table, marked with "union": true. This is the single point where tags register, so a tag that is only
	// ever referenced (never defined in this header) still gets an entry -- SA
	// needs that to register it as an incomplete/opaque struct type rather than
	// leaving the tag name unresolved.
	vector<json> capturedStructs_;
	map<string, int> structIndex_;  // tag name -> index into capturedStructs_
	// `body` holds "fields" and any struct-level "packed"/"aligned".
	void captureStructTag(const string &name, const json *body, bool is_union);
	string synthesizeAnonTag(const CToken* at);

	// Every enum body seen while parsing, in first-appearance order. An
	// anonymous body is named by synthesizeAnonTag until a typedef renames it;
	// one no typedef names stays "anonymous", and C code references its
	// enumerators unqualified. A body with an enumerator whose value can't be
	// computed is not "evaluable" and is never exported.
	struct CapturedEnum {
		string name;
		bool anonymous;
		bool evaluable;
		vector<pair<string, long long>> values;
	};
	vector<CapturedEnum> capturedEnums_;
	map<string, int> enumIndex_;  // enum name -> index into capturedEnums_
	// Enumerators share C's ordinary identifier scope, so a constant expression
	// anywhere later in the header can reference them.
	struct Enumerator { long long value; int enumIndex; };
	map<string, Enumerator> enumerators_;
	void nameAnonEnumByTypedef(json &vt, const string &typedefName);

	// The tables as loadState left them, by name: an entry still equal to its
	// inherited one was exported by the earlier run, so parse() leaves it out.
	map<string, json> inheritedTypedefs_, inheritedStructs_, inheritedEnums_;
	json enumState(const CapturedEnum &e) const;

	int parse(json &ast, const vector<CToken*>& tokens);

	bool declaration(json &ast, const vector<CToken*> &tokens, int &index, bool is_top_level);
	void emitDeclarator(json &ast, json &decl,
			bool is_typedef, bool is_static, bool is_extern, bool is_top_level);
	bool declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool type_specifier(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool declarator(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_typeonly);
	bool declarator_tail(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool parameter_list(vector<json> &params, const vector<CToken*> &tokens, int &result_index);
	bool struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_struct);
	bool enum_definition(string &name, const vector<CToken*> &tokens, int &result_index);
	bool attribute_specifiers(json &attrs, const vector<CToken*> &tokens, int &result_index);


	// This subtree (primary_expression .. expression) builds an expression tree into
	// `value`; see CParser.cpp.
	bool primary_expression(json &value, const vector<CToken*> &tokens, int &index);
	bool postfix_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool unary_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool cast_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool multiplicative_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool additive_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool shift_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool relational_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool equality_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool and_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool exclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool inclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool logical_and_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool logical_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool conditional_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool constant_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool assignment_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool expression(json &value, const vector<CToken*> &tokens, int &result_index);

	bool binaryLevel(json &value, const vector<CToken*> &tokens, int &result_index,
			std::initializer_list<int> ops,
			bool (CParser::*next)(json &, const vector<CToken*> &, int &));
	bool adjacentPuncs(const vector<CToken*> &tokens, int index, int first, int second) const;
	json foldConst(const json &tree);
	bool startsTypeName(const vector<CToken*> &tokens, int index) const;
	// The parameters of the function-like macro whose body is being parsed.
	const vector<string> *macroParams_ = nullptr;

	bool resolveConstValue(const json &node, json &value, json &type);
	bool typeSizeAlign(const json &vt, long long &size, long long &align);
	bool exportFuncAlias(json &ast, const string &name, const string &target);
	json expandableDefinition(CMacro *m, CPreprocessor &cpp);

	// void debug_token(const CToken* token);

public:
	CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers);
	int parse(json &ast);
	void exportMacroConstants(json &ast, const vector<CMacro*> &macros, CPreprocessor &cpp);
	// The tables a later run starts from, in the c-state.
	json dumpState() const;
	void loadState(const json &state);
};
