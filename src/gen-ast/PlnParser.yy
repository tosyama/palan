/// Palan Parser
///
/// @file PlnParser.yy
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

%glr-parser
%language "c++"
%require "3.8"
%skeleton "glr2.cc"

%defines
%define api.parser.class	{PlnParser}
%parse-param	{PlnLexer& lexer}	{json& ast}	{MacroTable& macros}	{CStateHistory& cState}	{const string& buildTarget}
%lex-param	{PlnLexer& lexer}

%code requires
{
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <stdexcept>

#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnGenAstMacroFold.h"
#include "PlnGenAstC2Ast.h"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::runtime_error;
using json = nlohmann::json;
namespace fs = std::filesystem;

class PlnLexer;

// glr2.cc passes $N to actions as const references (GLR stacks may share a
// value), so `$$ = move($1); $$.push_back(x)` copies the whole list on every
// append. An immutable list sharing its prefix makes each append O(1).
template <class T> struct PList {
	struct Node {
		T item;
		// mutable only so the destructor can unlink the chain iteratively;
		// the default recursive release overflows the stack on long lists.
		mutable std::shared_ptr<const Node> prev;
		~Node() {
			auto p = std::move(prev);
			while (p && p.use_count() == 1)
				p = std::move(p->prev);
		}
	};
	std::shared_ptr<const Node> last;
	size_t size = 0;

	PList push(T item) const { return { std::make_shared<const Node>(Node{ std::move(item), last }), size + 1 }; } // LCOV_EXCL_BR_LINE

	vector<T> toVector() const {
		vector<T> v(size);
		const Node* n = last.get();
		for (size_t i = size; i > 0; --i, n = n->prev.get()) v[i-1] = n->item;
		return v;
	} // LCOV_EXCL_LINE

	template <class Pred> const T* findLast(Pred pred) const {
		for (const Node* n = last.get(); n; n = n->prev.get())
			if (pred(n->item)) return &n->item;
		return nullptr;
	}
};

// A statement holds its nested blocks by handle and is turned into json once
// at the end; building json at each reduction would copy a block's contents
// once per enclosing level.
struct LazyNode;
using NodeRef = std::shared_ptr<const LazyNode>;
struct LazyNode {
	json self;
	vector<std::pair<string, NodeRef>> nodes;
	vector<std::pair<string, PList<NodeRef>>> lists;

	json toJson() const {
		json j = self;
		for (auto& [key, node] : nodes) j[key] = node->toJson();
		for (auto& [key, list] : lists) j[key] = toJsonArray(list);
		return j;
	} // LCOV_EXCL_LINE
	static json toJsonArray(const PList<NodeRef>& list) {
		json arr = json::array();
		for (auto& n : list.toVector()) arr.push_back(n->toJson());
		return arr;
	} // LCOV_EXCL_LINE
};

inline NodeRef leafNode(json j) { return std::make_shared<const LazyNode>(LazyNode{ std::move(j), {}, {} }); } // LCOV_EXCL_BR_LINE
inline NodeRef blockStmtNode(const PList<NodeRef>& body) {
	return std::make_shared<const LazyNode>(LazyNode{ {{"stmt-type", "block"}}, {}, {{"body", body}} }); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// A `global` declaration with a `for` list is two statements (a json array).
inline PList<NodeRef> pushStmt(const PList<NodeRef>& list, json stmt) {
	if (!stmt.is_array()) return list.push(leafNode(std::move(stmt)));
	PList<NodeRef> l = list;
	for (auto& s : stmt) l = l.push(leafNode(std::move(s)));
	return l;
}

struct BodyList {
	PList<NodeRef> functions, body;
	NodeRef toNode() const {
		return std::make_shared<const LazyNode>(LazyNode{ json::object(), {}, {{"functions", functions}, {"body", body}} }); // LCOV_EXCL_BR_LINE
	} // LCOV_EXCL_BR_LINE
};
}

%code
{
	#include <set>
	#include "PlnLexer.h"
	#include "PlnGenAstMessage.h"
	#include "PlnGenAstInternal.h"
	#include "../common/PlnDefs.h"
	#include "../common/PlnStrLit.h"

	static std::set<std::string> typeNames = {
		"int8",  "int16",  "int32",  "int64",
		"uint8", "uint16", "uint32", "uint64",
		"flo32", "flo64", "flo80", "bool"
	};

	int yylex(
		palan::PlnParser::value_type* yylval,
		palan::PlnParser::location_type* location,
		PlnLexer& lexer)
	{
		return lexer.yylex(*yylval, *location);
	}

	// A header cincluded in a block is read again by a later cinclude outside
	// it, and its macros fold only inside it.
	static void closeCIncludeScope(CStateHistory& cState, MacroTable& macros,
		const palan::PlnParser::location_type& open, const palan::PlnParser::location_type& close)
	{
		if (!cState.closeScope({open.begin.line, open.begin.column}, {close.begin.line, close.begin.column}))
			return;
		closeMacroScope(macros, {(int)open.begin.line, (int)open.begin.column}, {(int)close.begin.line, (int)close.begin.column});
	} // LCOV_EXCL_BR_LINE

// glr2.cc grows the GLR stack by memcpy-ing items and then destroying the
// originals, which frees each semantic value twice. Reserve the maximum depth
// up front and never grow; a deeper parse reports "memory exhausted".
#define YYMAXDEPTH 10000
#define YYINITDEPTH YYMAXDEPTH
#define YYSTACKEXPANDABLE 0

#define LOC(J, L)       J["loc"] = { (int)L.begin.line, (int)L.begin.column, (int)L.end.line, (int)L.end.column }
#define LOC_BE(J, B, E) J["loc"] = { (int)B.begin.line, (int)B.begin.column, (int)E.end.line, (int)E.end.column }

// An empty rule (e.g. arguments) is located at the end of the previous token,
// so a rule's start is taken from its first non-empty component instead.
#define YYLLOC_DEFAULT(Current, Rhs, N)                                   \
	do {                                                                  \
		if (N) {                                                          \
			int yyk = 1;                                                  \
			while (yyk < (N) && YYRHSLOC(Rhs, yyk).begin.line == YYRHSLOC(Rhs, yyk).end.line \
			       && YYRHSLOC(Rhs, yyk).begin.column == YYRHSLOC(Rhs, yyk).end.column) \
				yyk++;                                                    \
			(Current).begin = YYRHSLOC(Rhs, yyk).begin;                   \
			(Current).end   = YYRHSLOC(Rhs, N).end;                       \
		} else {                                                          \
			(Current).begin = (Current).end = YYRHSLOC(Rhs, 0).end;       \
		}                                                                 \
	} while (false)

	// Only a single typed variable can be global; anything else stays
	// unimplemented for SA to report.
	static json globalDecl(const json& var, const palan::PlnParser::location_type& loc, bool exported = false)
	{
		json j;
		if (var.contains("var-type") && !var.contains("not-impl")) {
			j = {{"stmt-type", "var-decl"}, {"global", true}, {"vars", json::array({var})}}; // LCOV_EXCL_BR_LINE
			if (exported) j["export"] = true; // LCOV_EXCL_BR_LINE
		} else
			j = {{"stmt-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
		LOC(j, loc); // LCOV_EXCL_BR_LINE
		return j;
	} // LCOV_EXCL_BR_LINE

	static json globalPermit(const string& name, const PList<json>& funcs,
	                         const palan::PlnParser::location_type& loc)
	{
		json j = {{"stmt-type", "global-permit"}, {"name", name}, {"funcs", funcs.toVector()}}; // LCOV_EXCL_BR_LINE
		LOC(j, loc); // LCOV_EXCL_BR_LINE
		return j;
	} // LCOV_EXCL_BR_LINE

	// Only a top-level cinclude is part of what a module publishes.
	static void rejectCIncludeExport(const json& node, const string& file)
	{
		if (node.is_object() && node.value("stmt-type", "") == "cinclude") {
			if (node.contains("export-types"))
				throw runtime_error(PlnGenAstMessage::locatedError(file, node["loc"][0], node["loc"][1],
					PlnGenAstMessage::getMessage(E_NestedCIncludeExport)));
			return;
		}
		if (node.is_structured())
			for (auto& child : node) rejectCIncludeExport(child, file);
	}

	// Defining one would let it disagree with the operator it is derived from.
	[[noreturn]] static void derivedCompare(const PlnLexer& lexer, const palan::PlnParser::location_type& loc,
	                                        const string& op, const string& base)
	{
		throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, loc.begin.line, loc.begin.column,
			PlnGenAstMessage::getMessage(E_DerivedCompareFunc, op, base)));
	}

	static json libraryImport(json pathInf, const string& file, const palan::PlnParser::location_type& loc)
	{
		if (pathInf["path-type"] == "inc" && !isLibraryImportPath(pathInf["path"]))
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_BadLibraryImport, pathInf["path"])));
		return resolveImportPath(move(pathInf), file);
	}

	// `libpath`, `link`, `built` and `target` stay identifiers elsewhere, like 'in'. None is a
	// statement of the program, so no node comes out (an empty statement list).
	static json interfaceStmt(json& ast, const string& word, const string& value, const string& file,
	                          const string& buildTarget, const palan::PlnParser::location_type& loc)
	{
		auto fail = [&](PlnMessageCode code, const string& arg) {
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(code, arg)));
		};
		if (word == "libpath") {
			if (isInterfaceFile(file)) fail(E_LibpathInInterface, "");
			auto dir = std::filesystem::path(file).parent_path() / value;
			ast["libpaths"].push_back(std::filesystem::weakly_canonical(dir).string());
			return json::array();
		}
		if (word != "link" && word != "built" && word != "target") fail(E_UnexpectedString, word);
		if (!isInterfaceFile(file)) fail(E_InterfaceOnly, "'" + word + "'");
		if (word == "link") {
			ast["libs"].push_back(value);
		} else if (word == "target") {
			if (value != buildTarget) fail(E_InterfaceOtherTarget, value);
			ast["target"] = value;
		} else {
			if (value != PALAN_VERSION) fail(E_InterfaceOtherVersion, value);
			ast["built"] = value;
		}
		return json::array();
	} // LCOV_EXCL_BR_LINE

	static string fieldAccess(const string& file, const string& word,
	                          const palan::PlnParser::location_type& loc)
	{
		if (word != "private" && word != "readonly")
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_UnknownFieldMarker, word)));
		return word;
	}

	static void requireCallValue(const string& file, const json& value, const palan::PlnParser::location_type& loc)
	{
		string et = value.value("expr-type", "");
		if (et != "call" && et != "member-call" && et != "chain-call")
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_ReceiveFromNonCall)));
	}

	static void rejectChainAfterReceive(const string& file, const json& value, const palan::PlnParser::location_type& arrow)
	{
		if (value.value("stmt-type", "") == "tapple-assign")
			throw runtime_error(PlnGenAstMessage::locatedError(file, arrow.begin.line, arrow.begin.column,
				PlnGenAstMessage::getMessage(E_ChainAfterReceive)));
	}

	[[noreturn]] static void variadicNotLast(PlnLexer& lexer, const palan::PlnParser::location_type& loc)
	{
		throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, loc.begin.line, loc.begin.column,
			PlnGenAstMessage::getMessage(E_VariadicNotLast)));
	}

	// Only an interface may leave an opaque type's contents out: a source
	// declaring the type must say what it holds.
	static json opaqueOfInterface(const string& file, const string& name, bool exported,
	                              const palan::PlnParser::location_type& loc)
	{
		if (!isInterfaceFile(file))
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_InterfaceOnly, "an opaque type without its contents")));
		json t = {{"name", name}, {"opaque", nullptr}}; // LCOV_EXCL_BR_LINE
		if (exported) t["export"] = true;
		LOC(t, loc); // LCOV_EXCL_BR_LINE
		return t;
	} // LCOV_EXCL_BR_LINE

	// `copy` stays an identifier elsewhere, as in a `for`.
	static void requireCopyWord(const string& file, const string& word, const string& variant,
	                            const palan::PlnParser::location_type& loc)
	{
		if (word != "copy")
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_ExpectedIsBinding, variant, word)));
	}

	// The variable an `is` binds has the shape of a `for` loop variable.
	static json isBinding(const string& variant, const string& name, bool borrow, bool mut,
	                      const palan::PlnParser::location_type& loc)
	{
		json bind = {{"name", name}, {"borrow", borrow}}; // LCOV_EXCL_BR_LINE
		if (borrow) bind["mutable"] = mut;
		LOC(bind, loc); // LCOV_EXCL_BR_LINE
		return {{"variants", json::array({variant})}, {"bind", move(bind)}}; // LCOV_EXCL_BR_LINE
	} // LCOV_EXCL_BR_LINE

	static NodeRef ifIsNode(const json& value, const PList<NodeRef>& arms, NodeRef elseBlock,
	                        const palan::PlnParser::location_type& loc)
	{
		json i = {{"stmt-type", "if-is"}, {"value", value}}; // LCOV_EXCL_BR_LINE
		LOC(i, loc); // LCOV_EXCL_BR_LINE
		vector<std::pair<string, NodeRef>> nodes;
		if (elseBlock) nodes.push_back({"else", elseBlock});
		return std::make_shared<const LazyNode>(LazyNode{ move(i), move(nodes), {{"arms", arms}} }); // LCOV_EXCL_BR_LINE
	} // LCOV_EXCL_BR_LINE

	// A string key stays a literal expression, so palan-sa reads it as any other string literal.
	static json strKeyItem(const string& bytes, const palan::PlnParser::location_type& keyLoc, const json& value,
	                       const palan::PlnParser::location_type& begin, const palan::PlnParser::location_type& end)
	{
		json key = {{"expr-type", "lit-str"}, {"value", strLitText(bytes)}}; // LCOV_EXCL_BR_LINE
		LOC(key, keyLoc); // LCOV_EXCL_BR_LINE
		json item = {{"key", move(key)}, {"value", value}}; // LCOV_EXCL_BR_LINE
		LOC_BE(item, begin, end); // LCOV_EXCL_BR_LINE
		return item;
	} // LCOV_EXCL_BR_LINE

	[[noreturn]] static void frameParamOrRet(const string& file, const palan::PlnParser::location_type& loc)
	{
		throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
			PlnGenAstMessage::getMessage(E_FrameParamOrRet)));
	}

	// A parameter or return the compiler does not implement stays in the
	// signature as is, so SA can reject the function where it is defined.
	static json funcDecl(json& ast, const string& file, bool exported, json fn, const vector<json>& params,
	                     const json& ret, bool release, const palan::PlnParser::location_type& loc,
	                     bool cExport = false)
	{
		// Parameter entries carry no location of their own, so a misplaced '$'
		// is reported at the function. A name after only unimplemented entries
		// has no type to inherit and stays an unimplemented entry for SA.
		auto rejectUnimplemented = [&](vector<json> vars) {
			for (auto& v : vars) {
				if (v.contains("var-type") && v["var-type"].value("type-kind", "") == "embed")
					frameParamOrRet(file, loc);
				if (v.value("inherit-type", false)) {
					v = {{"not-impl", true}}; // LCOV_EXCL_BR_LINE
					LOC(v, loc); // LCOV_EXCL_BR_LINE
				}
			}
			return vars;
		}; // LCOV_EXCL_BR_LINE
		fn["parameters"] = rejectUnimplemented(params);
		if (ret.contains("rets"))
			fn["rets"] = rejectUnimplemented(ret["rets"].get<vector<json>>());
		else if (ret.contains("ret-type"))
			fn["ret-type"] = ret["ret-type"];
		if (release) fn["release"] = true;
		LOC(fn, loc); // LCOV_EXCL_BR_LINE
		if (exported) {
			fn["export"] = true;
			if (cExport) fn["c-export"] = true;
			ast["export"].push_back(fn);
		}
		return fn;
	} // LCOV_EXCL_BR_LINE
}

%locations
%define api.namespace	{palan}
%define api.value.type	variant
%define parse.error	verbose

%token <string>	INT	"integer"
%token <string>	HEX	"hexadecimal integer"
%token <string>	CHAR_LIT	"character literal"
%token <string>	UINT	"unsigned integer"
%token <string>	FLO	"float"
%token <string>	STRING	"string"
%token <string>	ID	"identifier"
%token <string>	PATH	"path"
%token <string>	INCLUDE_FILE	"include file"
%token KW_LIBRARY	"library"
%token KW_EXPORT	"export"
%token KW_CEXPORT	"cexport"
%token KW_IMPORT	"import"
%token KW_CINCLUDE	"cinclude"
%token KW_FROM	"from"
%token KW_AS	"as"
%token KW_FUNC	"func"
%token KW_TYPE	"type"
%token KW_ENUM	"enum"
%token KW_OPAQUE	"opaque"
%token KW_UNION	"union"
%token KW_IS	"is"
%token KW_RELEASE	"release"
%token KW_CONST	"const"
%token KW_GLOBAL	"global"
%token KW_VOID	"void"
%token KW_RETURN	"return"
%token KW_FOR	"for"
%token KW_IN	"in"
%token KW_WHILE	"while"
%token KW_IF	"if"
%token KW_ELSE	"else"
%token KW_BREAK	"break"
%token KW_CONTINUE	"continue"
%token KW_SYSCALL	"syscall"
%token KW_SIZEOF	"sizeof"
%token KW_TRUE	"true"
%token KW_FALSE	"false"
%token OPE_LE	"<="
%token OPE_GE	">="
%token DBL_GRTR	">>"
%token OPE_SHL	"<<<"
%token OPE_SHR	">>>"
%token OPE_APPEND	"<<"
%token ARROW	"->"
%token DBL_ARROW	"->>"
%token OPE_TAKE	"<<="
%token AT_EXCL	"@!"
%token OPE_EQ	"=="
%token OPE_NE	"!="
%token OPE_AND	"&&"
%token OPE_OR	"||"
%token ELLIPSIS	"..."

%type <PList<NodeRef>>	statements block
%type <NodeRef>	block_stmt
%type <json>	expr_stmt
%type <PList<NodeRef>>	stmt_list_e stmt_list_b
%type <BodyList>	body_list_e body_list_b
%type <json>	import cinclude import_path
%type <PList<string>>	import_ids link_libs c_type_names cinclude_export
%type <vector<string>>	link_clause
%type <string>	field_name import_as func_name op_func_name library_decl library_name
%type <json>	expression value term store_loc argument
%type <PList<json>>	arguments
%type <json>	array_desc array_row dict_desc empty_braces
%type <PList<json>>	array_rows array_items dict_items
%type <json>	type_expr fptr_ret
%type <PList<json>>	fptr_params fptr_param_list
%type <json>	variadic_type variadic_param
%type <json>	var_declaration inherit_var_decl
%type <PList<json>>	var_declarations global_funcs
%type <json>	global_func
%type <NodeRef>	func_def func_item syscall_decl
%type <json>	return_def syscall_ret ret_type
%type <bool>	release_mark func_export
%type <json>	return
%type <vector<json>>	paramaters
%type <PList<json>>	expressions
%type <NodeRef>	block_obj standalone_block block_body_items
%type <json>	tapple_decl receive_target
%type <PList<json>>	receive_targets
%type <PList<json>>	tapple_decl_inner
%type <NodeRef>	if_stmt else_stmt while_loop for_loop
%type <json>	for_elem
%type <json>		type_decl type_member const_decl
%type <PList<json>>	type_members enumerators
%type <json>	enumerator
%type <json>	variant is_pattern bind_pattern
%type <PList<json>>	variants
%type <PList<string>>	arm_names
%type <NodeRef>	is_arm
%type <PList<NodeRef>>	is_arms

// A function pointer type's return is taken greedily: `e as @func(T) -> X`
// casts to a type returning X rather than chaining the cast into X.
%precedence FPTR_NORET
%left OPE_OR
%left OPE_AND
%left OPE_EQ OPE_NE
%precedence KW_IS
%left '<' '>' OPE_LE OPE_GE
%left ARROW DBL_ARROW
%left OPE_SHL OPE_SHR OPE_APPEND
%left '+' '-'
%left '*' '/' '%' '&' '|' '^'
%left KW_AS
%left DBL_GRTR
%right UNARY_MINUS '!' '~'
%left '.'

%start module

%%
module: library_decl statements
	{
		ast["module"] = moduleIdentity($1, lexer.inputFile);
		if (!ast["ast"].contains("functions"))
			ast["ast"]["functions"] = json::array();
		ast["ast"]["statements"] = LazyNode::toJsonArray($2);
		for (auto& stmt : ast["ast"]["statements"])
			if (stmt.value("stmt-type", "") != "cinclude") rejectCIncludeExport(stmt, lexer.inputFile);
		rejectCIncludeExport(ast["ast"]["functions"], lexer.inputFile);
		if (isInterfaceFile(lexer.inputFile) && !ast.contains("built"))
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, 1, 1,
				PlnGenAstMessage::getMessage(E_InterfaceNoVersion)));
		if (isInterfaceFile(lexer.inputFile) && !ast.contains("target"))
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, 1, 1,
				PlnGenAstMessage::getMessage(E_InterfaceNoTarget)));
	}
	;

library_decl: /* empty */
	{ }
	| KW_LIBRARY library_name ';'
	{ $$ = $2; }
	;

library_name: ID
	{ $$ = $1; }
	| library_name '.' ID
	{ $$ = $1 + "." + $3; }
	;

statements: /* empty */
	{ }
	| stmt_list_e
	{ $$ = $1; }
	| stmt_list_b
	{ $$ = $1; }
	| stmt_list_e ';'
	{ $$ = $1; }
	;

block_stmt: standalone_block
	{ $$ = $1; }
	| for_loop
	{ $$ = $1; }
	| while_loop
	{ $$ = $1; }
	| if_stmt
	{ $$ = $1; }
	;

expr_stmt: import
	{
		$$ = move($1);
		$$["stmt-type"] = "import";
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| cinclude
	{
		$$ = move($1);
		$$["stmt-type"] = "cinclude";

		json c_ast = execute_c2ast($$["path-type"], $$["path"],
		                          fs::path(lexer.inputFile).parent_path().string(), cState.current(), buildTarget);
		if (c_ast.is_object() && c_ast.contains("c-state")) {
			cState.push({@$.begin.line, @$.begin.column}, c_ast["c-state"].dump());
			for (auto& header : c_ast["headers"]) ast["c-headers"].push_back(header);
		}
		if (c_ast.is_object() && c_ast.contains("ast")) {
			if (c_ast["ast"].contains("functions")) {
				$$["functions"] = move(c_ast["ast"]["functions"]);
			}
			if (c_ast["ast"].contains("enums")) {
				$$["enums"] = move(c_ast["ast"]["enums"]);
			}
			if (c_ast["ast"].contains("structs")) {
				$$["structs"] = move(c_ast["ast"]["structs"]);
			}
			if (c_ast["ast"].contains("globals")) {
				$$["globals"] = move(c_ast["ast"]["globals"]);
			}
			if (c_ast["ast"].contains("typedefs")) {
				$$["typedefs"] = move(c_ast["ast"]["typedefs"]);
			}
			if (c_ast["ast"].contains("expandables")) {
				$$["expandables"] = move(c_ast["ast"]["expandables"]);
			}
			json ended = endedMacroNames(macros, c_ast["ast"]);
			if (!ended.empty()) $$["ended-macros"] = move(ended);
		}
		LOC($$, @$); // LCOV_EXCL_BR_LINE
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("constants")) {
			registerMacroConstants(macros, c_ast["ast"]["constants"], $$["loc"]);
		}
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("unexported-macros")) {
			registerUnexportedMacros(macros, c_ast["ast"]["unexported-macros"], $$["loc"]);
		}
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("undefined-macros")) {
			registerUndefinedMacros(macros, c_ast["ast"]["undefined-macros"], $$["loc"]);
		}
		if ($$.contains("expandables")) {
			registerExpandableMacros(macros, $$["expandables"], $$["loc"]);
		}
	}
	| var_declarations
	{
		vector<json> vars = $1.toVector();
		// Detect a tapple-decl emitted by var_declaration
		if (vars.size() == 1 && vars[0].value("stmt-type", "") == "tapple-decl") { // LCOV_EXCL_BR_LINE
			$$ = move(vars[0]);
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else {
			bool all_ok = true;
			for (auto& v : vars) {
				if (v.count("not-impl") || v.value("stmt-type","") == "tapple-decl"
					|| v.value("inherit-type", false)) { all_ok = false; break; }
			}
			if (all_ok) {
				$$ = {{"stmt-type", "var-decl"}, {"vars", move(vars)}}; // LCOV_EXCL_BR_LINE
				LOC($$, @$); // LCOV_EXCL_BR_LINE
			} else {
				$$ = {{"stmt-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
				for (auto& v : vars)
					if (v.contains("untyped-var")) { $$["untyped-var"] = v["untyped-var"]; break; }
				LOC($$, @$); // LCOV_EXCL_BR_LINE
			}
		}
	}
	| KW_GLOBAL var_declaration
	{ $$ = globalDecl($2, @$); }
	| KW_GLOBAL var_declaration KW_FOR global_funcs
	{ $$ = json::array({globalDecl($2, @$), globalPermit($2.value("name", ""), $4, @$)}); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_GLOBAL var_declaration
	{ $$ = globalDecl($3, @$, true); }
	| KW_EXPORT KW_GLOBAL var_declaration KW_FOR global_funcs
	{ $$ = json::array({globalDecl($3, @$, true), globalPermit($3.value("name", ""), $5, @$)}); } // LCOV_EXCL_BR_LINE
	| KW_GLOBAL ID
	{ $$ = globalPermit($2, PList<json>(), @$); }
	| KW_GLOBAL ID KW_FOR global_funcs
	{ $$ = globalPermit($2, $4, @$); }
	| KW_GLOBAL ID '.' ID
	{ $$ = globalPermit($2 + "." + $4, PList<json>(), @$); } // LCOV_EXCL_BR_LINE
	| KW_GLOBAL ID '.' ID KW_FOR global_funcs
	{ $$ = globalPermit($2 + "." + $4, $6, @$); } // LCOV_EXCL_BR_LINE
	| ID STRING
	{ $$ = interfaceStmt(ast, $1, $2, lexer.inputFile, buildTarget, @$); }
	| const_decl
	{ $$ = {{"stmt-type", "const-decl"}, {"name", $1["name"]}, {"value", move($1["value"])}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| type_decl
	{
		if ($1.contains("alias-of")) {
			$$ = {{"stmt-type", "type-alias"}, {"name", $1["name"]}, {"type", move($1["alias-of"])}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else if ($1.contains("opaque")) {
			$$ = {{"stmt-type", "opaque-def"}, {"name", $1["name"]}}; // LCOV_EXCL_BR_LINE
			if (!$1["opaque"].is_null()) $$["contents"] = move($1["opaque"]);
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else if ($1.contains("variants")) {
			$$ = {{"stmt-type", "union-def"}, {"name", $1["name"]}, {"variants", move($1["variants"])}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else if ($1.contains("enumerators")) {
			$$ = {{"stmt-type", "enum-def"}, {"name", $1["name"]}, {"enumerators", move($1["enumerators"])}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"stmt-type", "struct-def"}, {"name", $1["name"]}, {"fields", move($1["fields"])}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
		if ($1.contains("export")) $$["export"] = true;
	}
	| expression
	{
		if (isStoreStmt($1)) {
			$$ = move($1);
		} else {
			$$ = {{"stmt-type", "expr"}, {"body", move($1)}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| return
	{
		$$ = {{"stmt-type", "return"}}; // LCOV_EXCL_BR_LINE
		if ($1.contains("values"))
			$$["values"] = move($1["values"]);
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| KW_BREAK
	{ $$ = {{"stmt-type", "break"}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_CONTINUE
	{ $$ = {{"stmt-type", "continue"}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

stmt_list_e: expr_stmt
	{ $$ = pushStmt(PList<NodeRef>(), move($1)); }
	| stmt_list_b expr_stmt
	{ $$ = pushStmt($1, move($2)); }
	| stmt_list_b ';' expr_stmt
	{ $$ = pushStmt($1, move($3)); }
	| stmt_list_e ';' expr_stmt
	{ $$ = pushStmt($1, move($3)); }
	;

stmt_list_b: block_stmt
	{ $$ = PList<NodeRef>().push($1); }
	| func_item
	{ ast["ast"]["functions"].push_back($1->toJson()); }
	| stmt_list_b block_stmt
	{ $$ = $1.push($2); }
	| stmt_list_b func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($2->toJson());
	}
	| stmt_list_b ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_b ';' func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($3->toJson());
	}
	| stmt_list_e ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_e ';' func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($3->toJson());
	}
	;

import: KW_IMPORT import_path import_as
	{
		$$ = libraryImport($2, lexer.inputFile, @2);
		ast["import"].emplace_back($$);
		if ($3.size()) {
			$$["alias"] = $3;
		}
	}
	| KW_IMPORT import_ids KW_FROM import_path import_as
	{
		$$ = libraryImport($4, lexer.inputFile, @4);
		ast["import"].emplace_back($$);
		$$["targets"] = $2.toVector();
		if ($5.size()) { $$["alias"] = $5; }
	}
	; 

import_ids: func_name
	{ $$ = PList<string>().push($1); }
	| import_ids ',' func_name
	{ $$ = $1.push($3); }
	;

import_path: PATH
	{
		json pathinf = {
			{"path-type", "src"},
			{"path", $1}
		}; // LCOV_EXCL_BR_LINE
		$$ = move(pathinf);
	}
	| INCLUDE_FILE
	{
		json pathinf = {
			{"path-type", "inc"},
			{"path", $1}
		}; // LCOV_EXCL_BR_LINE
		$$ = move(pathinf);
	}
	;

import_as: /* empty */
	{ $$ = ""; }
	| KW_AS ID
	{ $$ = move($2); }
	;

cinclude: KW_CINCLUDE import_path import_as cinclude_export link_clause
	{
		$$ = move($2);
		if ($3.size()) {
			$$["alias"] = $3;
		}
		if ($4.size) $$["export-types"] = $4.toVector();
		json libs = json::array(), objs = json::array();
		for (auto& name : $5) {
			if (isLinkObjName(name))
				objs.push_back(resolveLinkObjPath(name, lexer.inputFile));
			else
				libs.push_back(name);
		}
		if (libs.size()) $$["libs"] = move(libs);
		if (objs.size()) $$["objs"] = move(objs);
	}
	;

cinclude_export: /* empty */
	{ }
	| KW_EXPORT c_type_names
	{ $$ = $2; }
	;

c_type_names: ID
	{ $$ = PList<string>().push($1); }
	| c_type_names ',' ID
	{ $$ = $1.push($3); }
	;

link_clause: /* empty */
	{ }
	| ID link_libs
	{
		if ($1 != "link") {
			throw runtime_error(
				PlnGenAstMessage::getMessage(E_ExpectedLinkKeyword, $1));
		}
		$$ = $2.toVector();
	}
	;

link_libs: STRING
	{ $$ = PList<string>().push($1); }
	| link_libs ',' STRING
	{ $$ = $1.push($3); }
	;

block: '{' statements '}'
	{ $$ = $2; closeCIncludeScope(cState, macros, @1, @3); }
	;

body_list_e: expr_stmt
	{
		$$.body = pushStmt($$.body, move($1));
	}
	| body_list_b expr_stmt
	{
		$$ = $1;
		$$.body = pushStmt($$.body, move($2));
	}
	| body_list_b ';' expr_stmt
	{
		$$ = $1;
		$$.body = pushStmt($$.body, move($3));
	}
	| body_list_e ';' expr_stmt
	{
		$$ = $1;
		$$.body = pushStmt($$.body, move($3));
	}
	;

body_list_b: block_stmt
	{
		$$.body = $$.body.push($1);
	}
	| func_item
	{ $$.functions = $$.functions.push($1); }
	| body_list_b block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($2);
	}
	| body_list_b func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($2);
	}
	| body_list_b ';' block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($3);
	}
	| body_list_b ';' func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($3);
	}
	| body_list_e ';' block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($3);
	}
	| body_list_e ';' func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($3);
	}
	;

block_body_items: /* empty */
	{ $$ = BodyList().toNode(); }
	| body_list_e
	{ $$ = $1.toNode(); }
	| body_list_b
	{ $$ = $1.toNode(); }
	| body_list_e ';'
	{ $$ = $1.toNode(); }
	;

block_obj: '{' block_body_items '}'
	{ $$ = $2; closeCIncludeScope(cState, macros, @1, @3); }
	;

standalone_block: block_obj
	{
		LazyNode blk = *$1;
		blk.self["stmt-type"] = "block";
		LOC(blk.self, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(std::move(blk));
	}
	;

var_declarations: var_declaration
	{ $$ = PList<json>().push($1); }
	| var_declarations ',' var_declaration   %dprec 1
	{ $$ = $1.push($3); }
	| var_declarations ',' inherit_var_decl  %dprec 2
	{
		json next = $3;
		if (auto typed = $1.findLast([](const json& v) { return v.contains("var-type"); })) {
			next["var-type"] = (*typed)["var-type"];
			next.erase("inherit-type");
		}
		$$ = $1.push(move(next));
	}
	;

global_funcs: global_func
	{ $$ = PList<json>().push($1); }
	| global_funcs ',' global_func
	{ $$ = $1.push($3); }
	;

global_func: func_name
	{ $$ = {{"name", $1}, {"writable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| func_name '!'
	{ $$ = {{"name", $1}, {"writable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

inherit_var_decl: ID
	{ $$ = {{"name", $1}, {"inherit-type", true}}; } // LCOV_EXCL_BR_LINE
	| ID '=' value
	{ $$ = {{"name", $1}, {"inherit-type", true}, {"init", $3}}; } // LCOV_EXCL_BR_LINE
	| ID OPE_TAKE value
	{ $$ = {{"name", $1}, {"inherit-type", true}, {"init", $3}, {"ownership-transfer", true}}; } // LCOV_EXCL_BR_LINE
	;

var_declaration: type_expr ID
	{
		if (isDeclarableVarType($1) || isFrameVarType($1)) {
			$$ = {{"name", $2}, {"var-type", move($1)}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"not-impl", true}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| type_expr ID '=' value
	{
		if (isDeclarableVarType($1) || isFrameVarType($1)) {
			$$ = {{"name", $2}, {"var-type", move($1)}, {"init", move($4)}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"not-impl", true}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| type_expr ID OPE_TAKE value
	{
		if (isDeclarableVarType($1) || isFrameVarType($1)) {
			$$ = {{"name", $2}, {"var-type", move($1)}, {"init", move($4)}, {"ownership-transfer", true}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"not-impl", true}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| ID '=' expression
	{ $$ = {{"not-impl", true}, {"untyped-var", $1}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| tapple_decl '=' expression
	{
		requireCallValue(lexer.inputFile, $3, @3);
		$$ = {{"stmt-type", "tapple-decl"}, {"vars", $1}, {"value", $3}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	;

tapple_decl: '(' tapple_decl_inner ')'
	{ $$ = $2.toVector(); }
	;

tapple_decl_inner: type_expr ID
	{ $$ = PList<json>().push({{"var-name", $2}, {"var-type", $1}}); } // LCOV_EXCL_BR_LINE
	| KW_VOID
	{ }
	| tapple_decl_inner ',' type_expr ID
	{ $$ = $1.push({{"var-name", $4}, {"var-type", $3}}); } // LCOV_EXCL_BR_LINE
	| tapple_decl_inner ',' KW_VOID
	{ $$ = $1; }
	| tapple_decl_inner ',' ID   %dprec 1
	{
	  if (auto typed = $1.findLast([](const json& v) { return v.contains("var-type"); }))
	      $$ = $1.push({{"var-name", $3}, {"var-type", (*typed)["var-type"]}}); // LCOV_EXCL_BR_LINE
	  else
	      $$ = $1;
	}
	;

const_decl: KW_CONST ID '=' expression
	{ $$ = {{"name", $2}, {"value", move($4)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

type_decl: KW_TYPE ID '{' type_members '}'
	{ $$ = {{"name", $2}, {"fields", $4.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_TYPE ID '{' type_members '}'
	{ $$ = {{"name", $3}, {"fields", $5.toVector()}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TYPE ID KW_ENUM '{' enumerators opt_comma '}'
	{ $$ = {{"name", $2}, {"enumerators", $5.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_TYPE ID KW_ENUM '{' enumerators opt_comma '}'
	{ $$ = {{"name", $3}, {"enumerators", $6.toVector()}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TYPE ID KW_UNION '{' variants '}'
	{ $$ = {{"name", $2}, {"variants", $5.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_TYPE ID KW_UNION '{' variants '}'
	{ $$ = {{"name", $3}, {"variants", $6.toVector()}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $2}, {"alias-of", move($4)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $3}, {"alias-of", move($5)}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TYPE ID '=' KW_OPAQUE type_expr
	{ $$ = {{"name", $2}, {"opaque", move($5)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_EXPORT KW_TYPE ID '=' KW_OPAQUE type_expr
	{ $$ = {{"name", $3}, {"opaque", move($6)}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TYPE ID '=' KW_OPAQUE
	{ $$ = opaqueOfInterface(lexer.inputFile, $2, false, @$); }
	| KW_EXPORT KW_TYPE ID '=' KW_OPAQUE
	{ $$ = opaqueOfInterface(lexer.inputFile, $3, true, @$); }
	;

enumerators: enumerator
	{ $$ = PList<json>().push($1); }
	| enumerators ',' enumerator
	{ $$ = $1.push($3); }
	;

enumerator: ID
	{ $$ = {{"name", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| ID '=' expression
	{ $$ = {{"name", move($1)}, {"value", move($3)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

// A variant's name also names the variable an `is` binds, so it is an identifier.
variants: variant
	{ $$ = PList<json>().push($1); }
	| variants variant
	{ $$ = $1.push($2); }
	;

variant: ID ';'
	{ $$ = {{"name", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| type_expr ID ';'
	{ $$ = {{"name", move($2)}, {"var-type", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

opt_comma: /* empty */
	| ','
	;

type_members: type_member
	{ $$ = PList<json>().push($1); }
	| type_members type_member
	{ $$ = $1.push($2); }
	;

type_member: type_expr field_name ';'
	{ $$ = {{"name", move($2)}, {"var-type", move($1)}}; } // LCOV_EXCL_BR_LINE
	| type_expr field_name '=' value ';'
	{ $$ = {{"name", move($2)}, {"var-type", move($1)}, {"init", move($4)}}; } // LCOV_EXCL_BR_LINE
	| ID type_expr field_name ';'
	{ $$ = {{"name", move($3)}, {"var-type", move($2)}, {"access", fieldAccess(lexer.inputFile, $1, @1)}}; } // LCOV_EXCL_BR_LINE
	| ID type_expr field_name '=' value ';'
	{ $$ = {{"name", move($3)}, {"var-type", move($2)}, {"init", move($5)}, {"access", fieldAccess(lexer.inputFile, $1, @1)}}; } // LCOV_EXCL_BR_LINE
	;

// 'in', 'is' and 'union' are reserved, but fields, C structs' among them, are still named 'in'.
field_name: ID
	{ $$ = move($1); }
	| KW_IN
	{ $$ = "in"; } // LCOV_EXCL_BR_LINE
	| KW_IS
	{ $$ = "is"; } // LCOV_EXCL_BR_LINE
	| KW_UNION
	{ $$ = "union"; } // LCOV_EXCL_BR_LINE
	;

expression: term
	{ $$ = move($1); }
	| array_desc
	{ $$ = move($1); }
	| dict_desc
	{ $$ = move($1); }
	| expression '+' expression
	{ $$ = {{"expr-type", "add"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '-' expression
	{ $$ = {{"expr-type", "sub"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '-' expression %prec UNARY_MINUS
	{ $$ = negateExpr(move($2)); LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '*' expression
	{ $$ = {{"expr-type", "mul"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '/' expression
	{ $$ = {{"expr-type", "div"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '%' expression
	{ $$ = {{"expr-type", "mod"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '&' expression
	{ $$ = {{"expr-type", "bitand"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '|' expression
	{ $$ = {{"expr-type", "bitor"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '^' expression
	{ $$ = {{"expr-type", "bitxor"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_SHL expression
	{ $$ = {{"expr-type", "shl"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_SHR expression
	{ $$ = {{"expr-type", "shr"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_APPEND expression
	{ $$ = {{"expr-type", "append"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression KW_AS type_expr
	{ $$ = {{"expr-type", "cast"}, {"target-type", move($3)}, {"src", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '~' expression %prec UNARY_MINUS
	{ $$ = {{"expr-type", "bitnot"}, {"operand", $2}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_LE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "<="}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_GE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", ">="}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '<' expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "<"},  {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression '>' expression
	{ $$ = {{"expr-type", "cmp"}, {"op", ">"},  {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_EQ expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "=="}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_NE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "!="}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_AND expression
	{ $$ = {{"expr-type", "logical-and"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression OPE_OR expression
	{ $$ = {{"expr-type", "logical-or"}, {"left", $1}, {"right", $3}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression KW_IS is_pattern
	{
		$$ = move($3);
		$$["expr-type"] = "is";
		$$["value"] = $1;
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| '!' expression
	{ $$ = {{"expr-type", "logical-not"}, {"operand", $2}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '@' store_loc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| AT_EXCL store_loc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '@' array_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| AT_EXCL array_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '@' dict_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| AT_EXCL dict_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '@' empty_braces %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| AT_EXCL empty_braces %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| expression ARROW store_loc
	{
		rejectChainAfterReceive(lexer.inputFile, $1, @2);
		if ($3.value("kind", "") == "call") {
			$$ = {{"expr-type", "chain-call"}, {"value", move($1)}, {"call", move($3["call"])}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = storeStmt(move($3), move($1), false);
		}
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| expression ARROW '(' receive_targets ')'
	{
		rejectChainAfterReceive(lexer.inputFile, $1, @2);
		requireCallValue(lexer.inputFile, $1, @1);
		$$ = {{"stmt-type", "tapple-assign"}, {"targets", $4.toVector()}, {"value", move($1)}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| expression DBL_ARROW store_loc
	{
		rejectChainAfterReceive(lexer.inputFile, $1, @2);
		string kind = $3.value("kind", "");
		if (kind == "var" || kind == "arr-index" || kind == "field") {
			$$ = storeStmt(move($3), move($1), true);
		} else if (kind == "call" && !isStoreStmt($1)) { // LCOV_EXCL_BR_LINE
			json value = {{"expr-type", "transfer"}, {"value", move($1)}, {"op", "->>"}}; // LCOV_EXCL_BR_LINE
			LOC(value, @1); // LCOV_EXCL_BR_LINE
			$$ = {{"expr-type", "chain-call"}, {"value", move(value)}, {"call", move($3["call"])}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"expr-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
		}
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	// `as` binds tighter than `->>`, so it is shifted here: a location takes it as
	// the owner it is viewed as, and a chained call's result is cast as before.
	| expression DBL_ARROW store_loc KW_AS type_expr
	{
		rejectChainAfterReceive(lexer.inputFile, $1, @2);
		string kind = $3.value("kind", "");
		if (kind == "var" || kind == "arr-index" || kind == "field") {
			$$ = storeStmt(move($3), move($1), true);
			if ($$.contains("stmt-type")) $$["owner-view-type"] = move($5);
		} else if (kind == "call" && !isStoreStmt($1)) { // LCOV_EXCL_BR_LINE
			json value = {{"expr-type", "transfer"}, {"value", move($1)}, {"op", "->>"}}; // LCOV_EXCL_BR_LINE
			LOC(value, @1); // LCOV_EXCL_BR_LINE
			json chain = {{"expr-type", "chain-call"}, {"value", move(value)}, {"call", move($3["call"])}}; // LCOV_EXCL_BR_LINE
			LOC_BE(chain, @1, @3); // LCOV_EXCL_BR_LINE
			$$ = {{"expr-type", "cast"}, {"target-type", move($5)}, {"src", move(chain)}}; // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"expr-type", "not-impl"}};
		}
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| expression DBL_GRTR
	{ $$ = {{"expr-type", "transfer"}, {"value", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

receive_targets: receive_target ',' receive_target
	{ $$ = PList<json>().push($1).push($3); }
	| receive_targets ',' receive_target
	{ $$ = $1.push($3); }
	;

receive_target: expression
	{
		string et = $1.value("expr-type", "");
		if (et != "id" && et != "arr-index" && et != "field-access")
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @1.begin.line, @1.begin.column,
				PlnGenAstMessage::getMessage(E_ReceiveTarget)));
		$$ = move($1);
	}
	;

term: INT
	{ $$ = {{"expr-type", "lit-int"}, {"value", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| HEX
	{ $$ = {{"expr-type", "lit-int"}, {"value", move($1)}, {"hex", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| CHAR_LIT
	{ $$ = {{"expr-type", "lit-int"}, {"value", move($1)}, {"char", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| UINT
	{ $$ = {{"expr-type", "lit-uint"}, {"value", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| FLO
	{ $$ = {{"expr-type", "lit-flo"}, {"value", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| STRING
	{ $$ = {{"expr-type", "lit-str"}, {"value", strLitText($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_TRUE
	{ $$ = boolLiteral("1"); LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_FALSE
	{ $$ = boolLiteral("0"); LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| KW_SIZEOF '(' type_expr ')'
	{
		if (isImplementedTypeExpr($3)) {
			$$ = {{"expr-type", "sizeof"}, {"type", move($3)}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		} else {
			$$ = {{"expr-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| ID
	{ $$ = {{"expr-type", "id"}, {"name", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '(' expression ')'
	{ $$ = $2; }
	| '(' expression ')' '(' arguments ')'
	{ $$ = {{"expr-type", "call-ptr"}, {"callee", move($2)}, {"args", $5.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| term '.' field_name
	{ $$ = {{"expr-type", "field-access"}, {"object", move($1)}, {"field", move($3)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| term '[' expression ']'
	{
		$$ = {{"expr-type", "arr-index"}, {"array", move($1)}, {"index", move($3)}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| ID '(' arguments ')'
	{
		if (typeNames.count($1)) {
			if ($3.size == 1) {
				$$ = {{"expr-type",   "cast"},
				      {"target-type", {{"type-kind", "prim"}, {"type-name", move($1)}}},
				      {"src",         $3.last->item}}; // LCOV_EXCL_BR_LINE
				LOC($$, @$); // LCOV_EXCL_BR_LINE
			} else {
				$$ = {{"expr-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
				LOC($$, @$); // LCOV_EXCL_BR_LINE
			}
		} else {
			$$ = {{"expr-type", "call"}, {"name", move($1)}, {"args", $3.toVector()}}; // LCOV_EXCL_BR_LINE
			LOC($$, @$); // LCOV_EXCL_BR_LINE
		}
	}
	| term '.' ID '(' arguments ')'
	{
		$$ = {{"expr-type", "member-call"},
		      {"object", move($1)},
		      {"method", move($3)},
		      {"args", $5.toVector()}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	;

arguments: /* empty */
	{ }
	| argument
	{ $$ = PList<json>().push($1); }
	| arguments ',' argument
	{ $$ = $1.push($3); }
	;

argument: value
	{ $$ = move($1); }
	| ELLIPSIS expression
	{ $$ = {{"expr-type", "spread"}, {"value", move($2)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

array_desc: array_row
	{ $$ = move($1); }
	| array_rows
	{
		// The concatenated form [a,b][c,d] yields the same AST as the nested [[a,b],[c,d]].
		$$ = {{"expr-type", "arr-lit"}, {"items", $1.toVector()}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	;

array_rows: array_row array_row
	{ $$ = PList<json>().push($1).push($2); }
	| array_rows array_row
	{ $$ = $1.push($2); }
	;

array_row: '[' array_items ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '[' array_items ',' ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '[' ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", json::array()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

array_items: value
	{ $$ = PList<json>().push($1); }
	| array_items ',' value
	{ $$ = $1.push($3); }
	;

// Only where a value goes: at the start of a statement '{}' is an empty block.
value: expression
	{ $$ = move($1); }
	| empty_braces
	{ $$ = move($1); }
	;

empty_braces: '{' '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", json::array()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

dict_desc: '{' dict_items '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '{' dict_items ',' '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

dict_items: field_name ':' value
	{
		json item = {{"name", move($1)}, {"value", move($3)}}; // LCOV_EXCL_BR_LINE
		LOC(item, @$); // LCOV_EXCL_BR_LINE
		$$ = PList<json>().push(move(item));
	}
	| dict_items ',' field_name ':' value
	{
		json item = {{"name", move($3)}, {"value", move($5)}}; // LCOV_EXCL_BR_LINE
		LOC_BE(item, @3, @5); // LCOV_EXCL_BR_LINE
		$$ = $1.push(move(item));
	}
	| STRING ':' value
	{
		$$ = PList<json>().push(strKeyItem($1, @1, $3, @1, @3));
	}
	| dict_items ',' STRING ':' value
	{
		$$ = $1.push(strKeyItem($3, @3, $5, @3, @5));
	}
	;

store_loc
	: ID
	{ $$ = {{"kind", "var"}, {"name", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| store_loc '[' expression ']'
	{
		json array_expr = storeLocToExpr($1);
		$$ = {{"kind", "arr-index"}, {"array", move(array_expr)}, {"index", move($3)}}; // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| store_loc '.' field_name
	{ $$ = {{"kind", "field"}, {"base", move($1)}, {"field", move($3)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| '(' expression ')'
	{ $$ = {{"kind", "expr"}, {"expr", move($2)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	// Not term's call rules: a term-rooted member call would let every `-> x[i]`
	// also parse as the object of a pending `.f()` and split the GLR parse
	// exponentially along an assignment chain.
	| ID '(' arguments ')'
	{
		if (typeNames.count($1))
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @1.begin.line, @1.begin.column,
				PlnGenAstMessage::getMessage(E_ConversionAsStoreLoc, $1)));
		$$ = {{"kind", "call"},
		      {"call", {{"expr-type", "call"}, {"name", move($1)}, {"args", $3.toVector()}}}}; // LCOV_EXCL_BR_LINE
		LOC($$["call"], @$); // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| store_loc '.' ID '(' arguments ')'
	{
		$$ = {{"kind", "call"},
		      {"call", {{"expr-type", "member-call"}, {"object", storeLocToExpr($1)},
		                {"method", move($3)}, {"args", $5.toVector()}}}}; // LCOV_EXCL_BR_LINE
		LOC($$["call"], @$); // LCOV_EXCL_BR_LINE
		LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	;

func_def: KW_FUNC func_name '(' paramaters ')' return_def release_mark block_obj
	{
		json fn = funcDecl(ast, lexer.inputFile, false, {{"name", $2}, {"func-type", "palan"}}, $4, $6, $7, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $8}}, {} }); // LCOV_EXCL_BR_LINE
	}
	| func_export KW_FUNC func_name '(' paramaters ')' return_def release_mark block_obj
	{
		json fn = funcDecl(ast, lexer.inputFile, true, {{"name", $3}, {"func-type", "palan"}}, $5, $7, $8, @$, $1); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $9}}, {} }); // LCOV_EXCL_BR_LINE
	}
	| func_export KW_FUNC func_name '(' paramaters ')' return_def release_mark ';'
	{
		if (!isInterfaceFile(lexer.inputFile))
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @$.begin.line, @$.begin.column,
				PlnGenAstMessage::getMessage(E_InterfaceOnly, "a function without a body")));
		$$ = leafNode(funcDecl(ast, lexer.inputFile, true, {{"name", $3}, {"func-type", "palan"}}, $5, $7, $8, @$, $1)); // LCOV_EXCL_BR_LINE
	}
	// A type's '->', which comes with the type rather than being exported.
	| KW_FUNC func_name '(' paramaters ')' return_def ';'
	{
		if (!isInterfaceFile(lexer.inputFile))
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @$.begin.line, @$.begin.column,
				PlnGenAstMessage::getMessage(E_InterfaceOnly, "a function without a body")));
		$$ = leafNode(funcDecl(ast, lexer.inputFile, false, {{"name", $2}, {"func-type", "palan"}}, $4, $6, false, @$)); // LCOV_EXCL_BR_LINE
	}
	;

func_export: KW_EXPORT
	{ $$ = false; }
	| KW_CEXPORT
	{ $$ = true; }
	;

release_mark: /* empty */
	{ $$ = false; }
	| KW_RELEASE
	{ $$ = true; }
	;

func_name: ID
	{ $$ = $1; }
	| op_func_name
	{ $$ = $1; }
	;

op_func_name: '+' { $$ = "+"; }
	| '-' { $$ = "-"; }
	| '*' { $$ = "*"; }
	| '/' { $$ = "/"; }
	| '%' { $$ = "%"; }
	| OPE_EQ { $$ = "=="; }
	| '<' { $$ = "<"; }
	| OPE_APPEND { $$ = "<<"; }
	| ARROW { $$ = "->"; }
	| OPE_NE { derivedCompare(lexer, @$, "!=", "=="); }
	| '>' { derivedCompare(lexer, @$, ">", "<"); }
	| OPE_LE { derivedCompare(lexer, @$, "<=", "<"); }
	| OPE_GE { derivedCompare(lexer, @$, ">=", "<"); }
	;

func_item: func_def
	{ $$ = $1; }
	| syscall_decl
	{ $$ = $1; }
	;

// A syscall declaration binds a number to a name callable like any other
// function (Linux syscall ABI, not System V -- see doc/SpecAndDesign.md).
syscall_decl: KW_SYSCALL ID '(' paramaters ')' syscall_ret '=' expression ';'
	{
		json fn = funcDecl(ast, lexer.inputFile, false, {{"name", $2}, {"func-type", "syscall"}, {"syscall-number", $8}}, $4, $6, false, @$); // LCOV_EXCL_BR_LINE
		$$ = leafNode(move(fn)); // LCOV_EXCL_BR_LINE
	}
	| KW_EXPORT KW_SYSCALL ID '(' paramaters ')' syscall_ret '=' expression ';'
	{
		json fn = funcDecl(ast, lexer.inputFile, true, {{"name", $3}, {"func-type", "syscall"}, {"syscall-number", $9}}, $5, $7, false, @$); // LCOV_EXCL_BR_LINE
		$$ = leafNode(move(fn)); // LCOV_EXCL_BR_LINE
	}
	;

paramaters: /* empty */
	{ }
	| var_declarations
	{ $$ = $1.toVector(); }
	| variadic_param
	{ $$ = {move($1)}; } // LCOV_EXCL_BR_LINE
	| var_declarations ',' variadic_param
	{ $$ = $1.push(move($3)).toVector(); }
	| variadic_param ',' var_declarations
	{ variadicNotLast(lexer, @1); }
	| var_declarations ',' variadic_param ',' var_declarations
	{ variadicNotLast(lexer, @3); }
	;

variadic_param: variadic_type ID
	{ $$ = {{"name", move($2)}, {"var-type", move($1)}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

variadic_type: '[' ELLIPSIS ']' type_expr
	{ $$ = {{"type-kind","arr"},{"specifier","variadic"},{"size-expr",nullptr},{"base-type",move($4)}}; } // LCOV_EXCL_BR_LINE
	;

return_def: /* empty */
	{ }
	| ARROW var_declarations
	{ $$["rets"] = $2.toVector(); }
	| ARROW ret_type
	{ $$["ret-type"] = move($2); }
	;

// Named returns get no "= expr" initializer here: a syscall has no body to
// use them in, and allowing one would collide with the syscall number's "=".
syscall_ret: /* empty */
	{ }
	| ARROW ret_type
	{ $$["ret-type"] = move($2); }
	| ARROW syscall_named_rets
	{
		throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile,
			@2.begin.line, @2.begin.column, PlnGenAstMessage::getMessage(E_SyscallNamedReturn)));
	}
	;

syscall_named_rets: type_expr ID
	| syscall_named_rets ',' type_expr ID
	| syscall_named_rets ',' ID
	;

ret_type: type_expr
	{
		if ($1.value("type-kind", "") == "embed")
			frameParamOrRet(lexer.inputFile, @1);
		if (isDeclarableVarType($1)) {
			$$ = move($1);
		} else {
			$$ = {{"not-impl", true}}; // LCOV_EXCL_BR_LINE
			LOC($$, @1); // LCOV_EXCL_BR_LINE
		}
	}
	;

return: KW_RETURN
	{ }
	| KW_RETURN expressions
	{ $$["values"] = $2.toVector(); }
	;

for_loop: KW_FOR for_elem KW_IN expression block
	{
		json f = {{"stmt-type", "for"}, {"elem", move($2)}, {"range", move($4)}}; // LCOV_EXCL_BR_LINE
		LOC(f, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(f), {}, {{"body", $5}} }); // LCOV_EXCL_BR_LINE
	}
	| KW_FOR for_elem ',' ID KW_IN expression block
	{
		json f = {{"stmt-type", "for"}, {"elem", move($2)}, {"index", $4}, {"range", move($6)}}; // LCOV_EXCL_BR_LINE
		LOC(f, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(f), {}, {{"body", $7}} }); // LCOV_EXCL_BR_LINE
	}
	| KW_FOR for_elem ',' type_expr ID KW_IN expression block
	{
		json f = {{"stmt-type", "for"}, {"elem", move($2)}, {"index", $5}, {"index-type", move($4)}, {"range", move($7)}}; // LCOV_EXCL_BR_LINE
		LOC(f, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(f), {}, {{"body", $8}} }); // LCOV_EXCL_BR_LINE
	}
	;

// 'copy' stays an identifier elsewhere, like 'in'. A bare loop variable is
// rejected so a write meant for the element never lands in a silent copy.
for_elem: ID
	{
		throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @1.begin.line, @1.begin.column,
			PlnGenAstMessage::getMessage(E_ForElemNoMode, $1)));
	}
	| ID ID
	{
		if ($1 != "copy")
			throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @1.begin.line, @1.begin.column,
				PlnGenAstMessage::getMessage(E_ExpectedCopyKeyword, $1)));
		$$ = {{"name", $2}, {"borrow", false}}; LOC($$, @$); // LCOV_EXCL_BR_LINE
	}
	| '@' ID
	{ $$ = {{"name", $2}, {"borrow", true}, {"mutable", false}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	| AT_EXCL ID
	{ $$ = {{"name", $2}, {"borrow", true}, {"mutable", true}}; LOC($$, @$); } // LCOV_EXCL_BR_LINE
	;

while_loop: KW_WHILE expression block
	{
		json w = {{"stmt-type", "while"}, {"cond", $2}}; // LCOV_EXCL_BR_LINE
		LOC(w, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(w), {}, {{"body", $3}} }); // LCOV_EXCL_BR_LINE
	}
	;

if_stmt: KW_IF expression block else_stmt
	{
		json i = {{"stmt-type", "if"}, {"cond", $2}}; // LCOV_EXCL_BR_LINE
		LOC(i, @$); // LCOV_EXCL_BR_LINE
		vector<std::pair<string, NodeRef>> children = {{"then", blockStmtNode($3)}}; // LCOV_EXCL_BR_LINE
		if ($4) children.push_back({"else", $4});
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(i), move(children), {} }); // LCOV_EXCL_BR_LINE
	}
	| KW_IF expression KW_IS '{' is_arms '}'
	{ $$ = ifIsNode($2, $5, nullptr, @$); }
	| KW_IF expression KW_IS '{' is_arms KW_ELSE block '}'
	{ $$ = ifIsNode($2, $5, blockStmtNode($7), @$); }
	;

is_pattern: ID
	{ $$ = {{"variants", json::array({$1})}}; } // LCOV_EXCL_BR_LINE
	| bind_pattern
	{ $$ = move($1); }
	;

bind_pattern: '@' ID
	{ $$ = isBinding($2, $2, true, false, @$); }
	| AT_EXCL ID
	{ $$ = isBinding($2, $2, true, true, @$); }
	| ID ID
	{ requireCopyWord(lexer.inputFile, $1, $2, @1); $$ = isBinding($2, $2, false, false, @$); }
	| '@' ID '=' ID
	{ $$ = isBinding($4, $2, true, false, @$); }
	| AT_EXCL ID '=' ID
	{ $$ = isBinding($4, $2, true, true, @$); }
	| ID ID '=' ID
	{ requireCopyWord(lexer.inputFile, $1, $4, @1); $$ = isBinding($4, $2, false, false, @$); }
	;

is_arms: is_arm
	{ $$ = PList<NodeRef>().push($1); }
	| is_arms is_arm
	{ $$ = $1.push($2); }
	;

is_arm: arm_names block
	{
		json arm = {{"variants", $1.toVector()}}; // LCOV_EXCL_BR_LINE
		LOC(arm, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(arm), {}, {{"body", $2}} }); // LCOV_EXCL_BR_LINE
	}
	| bind_pattern block
	{
		json arm = $1;
		LOC(arm, @$); // LCOV_EXCL_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(arm), {}, {{"body", $2}} }); // LCOV_EXCL_BR_LINE
	}
	;

arm_names: ID
	{ $$ = PList<string>().push($1); }
	| arm_names ',' ID
	{ $$ = $1.push($3); }
	;

else_stmt: /* empty */
	{ }
	| KW_ELSE block
	{ $$ = blockStmtNode($2); }
	| KW_ELSE if_stmt
	{ $$ = $2; }
	;

expressions: value
	{ $$ = PList<json>().push($1); }
	| expressions ',' value
	{ $$ = $1.push($3); }
	;

type_expr: ID
	{ $$ = {{"type-kind","prim"},{"type-name",move($1)}}; } // LCOV_EXCL_BR_LINE
	| ID '.' ID
	{ $$ = {{"type-kind","prim"},{"type-name",$1 + "." + $3}}; } // LCOV_EXCL_BR_LINE
	| '@' type_expr
	{ $$ = pntrTypeExpr(move($2), false); }
	| AT_EXCL type_expr
	{ $$ = pntrTypeExpr(move($2), true); }
	| '@' KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), false); }
	| AT_EXCL KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), true); }
	| '@' KW_FUNC '(' fptr_params ')' fptr_ret
	{ $$ = pntrTypeExpr(funcTypeExpr($4.toVector(), move($6)), false); }
	| AT_EXCL KW_FUNC '(' fptr_params ')' fptr_ret
	{ $$ = pntrTypeExpr(funcTypeExpr($4.toVector(), move($6)), true); }
	| '$' type_expr
	{ $$ = {{"type-kind","embed"},{"base-type",move($2)}}; } // LCOV_EXCL_BR_LINE
	| '[' expression ']' type_expr
	{
		// `[""]T` is read by this rule: a rule of its own would parse the same
		// input a second way.
		if ($2.value("expr-type","") == "lit-str") {
			if (!$2["value"].get<string>().empty())
				throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile, @2.begin.line, @2.begin.column,
					PlnGenAstMessage::getMessage(E_DictKeyTypeNotEmpty)));
			$$ = dictTypeExpr($4);
		} else if ($4.value("type-kind","") == "embed")
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",move($2)},{"embedded",true},{"base-type",move($4["base-type"])}}; // LCOV_EXCL_BR_LINE
		else
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",move($2)},{"base-type",move($4)}}; // LCOV_EXCL_BR_LINE
	}
	| '[' ']' type_expr
	{
		if ($3.value("type-kind","") == "embed")
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",nullptr},{"embedded",true},{"base-type",move($3["base-type"])}}; // LCOV_EXCL_BR_LINE
		else
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",nullptr},{"base-type",move($3)}}; // LCOV_EXCL_BR_LINE
	}
	| '[' '+' ']' type_expr
	{ $$ = varrTypeExpr(nullptr, move($4)); }
	| '[' '+' expression ']' type_expr
	{ $$ = varrTypeExpr(move($3), move($5)); }
	;

fptr_params: /* empty */
	{ }
	| fptr_param_list
	{ $$ = $1; }
	| variadic_type
	{ $$ = PList<json>().push({{"var-type", move($1)}}); } // LCOV_EXCL_BR_LINE
	| fptr_param_list ',' variadic_type
	{ $$ = $1.push({{"var-type", move($3)}}); } // LCOV_EXCL_BR_LINE
	| variadic_type ',' fptr_param_list
	{ variadicNotLast(lexer, @1); }
	| fptr_param_list ',' variadic_type ',' fptr_param_list
	{ variadicNotLast(lexer, @3); }
	;

fptr_param_list: type_expr
	{ $$ = PList<json>().push({{"var-type", fptrSigType(move($1))}}); } // LCOV_EXCL_BR_LINE
	| fptr_param_list ',' type_expr
	{ $$ = $1.push({{"var-type", fptrSigType(move($3))}}); } // LCOV_EXCL_BR_LINE
	;

fptr_ret: /* empty */ %prec FPTR_NORET
	{ $$ = voidTypeExpr(); }
	| ARROW type_expr
	{ $$ = fptrSigType(move($2)); }
	;


%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cerr << PlnGenAstMessage::locatedError(lexer.inputFile, l.begin.line, l.begin.column, m) << endl;
}

