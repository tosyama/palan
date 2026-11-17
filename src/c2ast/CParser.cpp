/// C parser: recursive descent parser for C declarations.
///
/// @file CParser.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <vector>
#include <list>
#include <set>
#include <string>
#include <iostream>
#include <utility>
#include <climits>
#include <cstdint>
#include <boost/assert.hpp>

using namespace std;

#include "CFileInfo.h"
#include "CToken.h"
#include "CLexer.h"
#include "CPreprocessor.h"
#include "CParser.h"
#include "PlnC2AstMessage.h"
#include "../common/PlnCBitField.h"

// void CParser::debug_token(const CToken* token)
// {
// 	CLexer* lexer = lexers[token->lexer_no];
// 	CToken0& token0 = lexer->tokens[token->token0_no];
// 	cout << lexer->infile.fname << ":" << token0.line_no << ":" << token0.pos+1 << ": ";
//
// 	switch (token->type) {
// 		case TT_ID:
// 			cout << "ID ";
// 			cout << *(token->info.id) << endl;
// 			break;
// 		case TT_KEYWORD:
// 			cout << "KEYWORD ";
// 			switch (token->info.keyword) {
// 				case TK_TYPEDEF:
// 					cout << "TYPEDEF" << endl;
// 					break;
// 				case TK_STRUCT:
// 					cout << "STRUCT" << endl;
// 					break;
// 				default:
// 					cout << "UNKNOWN_KEYWORD" << token->info.keyword << endl;
// 					break;
// 			}
// 			break;
// 		case TT_PUNCTUATOR:
// 			{
// 				cout << "PUNCTUATOR ";
// 				char c0 = token->info.punc & 0xFF;
// 				char c1 = (token->info.punc >> 8) & 0xFF;
// 				if (c1) cout << c1;
// 				cout << c0 << endl;
// 				break;
// 			}
// 		default:
// 			cout << "OTHER ";
// 			cout << token->type << endl;
// 			break;
// 	}
// }

CParser::CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers)
	: top_tokens(top_tokens), lexers(lexers)
{
}

// Single point where a struct/union tag registers into capturedStructs_, called
// from every place struct_union_definition() parses one (standalone declaration,
// type-specifier reference in a field/parameter/return type, and recursively
// for a nested struct-typed field) and, for an anonymous struct/union body, from
// declaration()'s tag synthesis (the typedef name stands in for the missing
// tag). `body` is null for a tag-only reference or forward declaration; a
// later full definition of the same tag promotes an existing tag-only entry
// in place (preserving its first-seen position in the output) rather than
// adding a duplicate. A first-seen full definition is never overwritten by a
// later one (first definition wins) -- declaration() relies on this by
// checking structIndex_ itself before calling, so a synthesized name never
// collides with an unrelated struct already holding it.
void CParser::captureStructTag(const string &name, const json *body, bool is_union)
{
	auto it = structIndex_.find(name);
	int idx;
	if (it == structIndex_.end()) {
		idx = (int)capturedStructs_.size();
		structIndex_[name] = idx;
		json entry = {{"name", name}}; // LCOV_EXCL_BR_LINE
		if (is_union) entry["union"] = true;
		capturedStructs_.push_back(move(entry));
	} else {
		idx = it->second;
	}
	json &entry = capturedStructs_[idx];
	if (!body || entry.contains("fields")) return;
	entry["fields"] = (*body)["fields"];
	for (const char *key : {"packed", "aligned"})
		if (body->contains(key)) entry[key] = (*body)[key];
} // LCOV_EXCL_BR_LINE

// Named after the body's source location rather than a per-run counter:
// each cinclude runs its own c2ast process and SA keeps the first complete
// definition of a name, so counters would collide across headers while a
// location names the same body identically in every run. Contains characters
// no C identifier can, so it never shadows a real tag.
string CParser::synthesizeAnonTag(const CToken* at)
{
	CLexer* lexer = lexers[at->lexer_no];
	const CToken0& t0 = lexer->tokens[at->token0_no];
	string base = "anon@" + lexer->infile.fname + ":" + to_string(t0.line_no)
		+ ":" + to_string(t0.pos + 1);
	// Tokens from one macro body share a location across expansions.
	string tag = base;
	for (int n = 2; structIndex_.count(tag) || enumIndex_.count(tag); n++)
		tag = base + "#" + to_string(n);
	return tag;
}

// Each form of a declaration backtracks to its start when it fails, so the
// position worth reporting is the furthest token any form examined. Kept per
// token vector: macro bodies and included files are parsed from other vectors.
static const vector<CToken*>* furthestTokens = nullptr;
static int furthestIndex = 0;

static void noteReached(const vector<CToken*> &tokens, int index)
{
	if (&tokens == furthestTokens && index > furthestIndex)
		furthestIndex = index;
}

bool consume(CTokenType expected_type, const vector<CToken*> &tokens, int &index) {
	noteReached(tokens, index);
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == expected_type) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME(t) consume(t, tokens, index)


bool consume_kw(CTokenKeyword expected_keyword, const vector<CToken*> &tokens, int &index) {
	noteReached(tokens, index);
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_KEYWORD && token->info.keyword == expected_keyword) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME_KW(kw) consume_kw(kw, tokens, index)

bool consume_punc(int expected_punc, const vector<CToken*> &tokens, int &index) {
	noteReached(tokens, index);
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_PUNCTUATOR && token->info.punc == expected_punc) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME_PUNC(punc) consume_punc(punc, tokens, index)

#define EXPECT_PUNC(punc) if (!CONSUME_PUNC(punc)) { return false; }

// Tries each punctuator in turn and consumes the first one that matches, returning
// its code (or 0, which no punctuator encodes, if none matched). Used by the binary
// expression levels below to consume one operator per left-associative loop iteration.
int consume_punc_any(std::initializer_list<int> puncs, const vector<CToken*> &tokens, int &index) {
	for (int p : puncs) {
		if (consume_punc(p, tokens, index)) return p;
	}
	return 0;
}
#define CONSUME_PUNC_ANY(...) consume_punc_any({__VA_ARGS__}, tokens, index)


const bool defalut_char_is_signed = true; 

bool unsigned_char(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if(CONSUME_KW(TK_CHAR))	{
			return true;
		}
		index--; // backtrack

	} else if (!defalut_char_is_signed) {
		return CONSUME_KW(TK_CHAR);
	}
	return false;
}

bool signed_char(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if(CONSUME_KW(TK_CHAR))	{
			return true;
		}
		index--; // backtrack

	} else if (defalut_char_is_signed) {
		return CONSUME_KW(TK_CHAR);
	}
	return false;
}

bool unsigned_long_long(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			if (CONSUME_KW(TK_LONG)) {
				CONSUME_KW(TK_INT); // optional
				return true;
			}
			index--; // backtrack
		}
		index--; // backtrack

	} 

	return false;
}

bool signed_long_long(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			if (CONSUME_KW(TK_LONG)) {
				CONSUME_KW(TK_INT); // optional
				return true;
			}
			index--; // backtrack
		}
		index--; // backtrack

	} 

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	return false;
}

bool unsigned_long(const vector<CToken*> &tokens, int &index)
{
	// do check for long long first
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_UNSIGNED)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}
	return false;
}

bool signed_long(const vector<CToken*> &tokens, int &index)
{
	// do check for long long first
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_LONG)) {
		CONSUME_KW(TK_INT); // optional
		return true;
	}
	return false;
}

bool unsigned_short(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_SHORT)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}
	return false;
}

bool signed_short(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_SHORT)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_SHORT)) {
		CONSUME_KW(TK_INT); // optional
		return true;
	}
	return false;
}

bool unsigned_int(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_INT)) {
			return true;
		}
		return true; // unsigned long/short/char should be processed before this function
	}
	return false;
}

bool signed_int(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_INT)) {
			return true;
		}
		return true; // signed long/short/char should be processed before this function
	}

	if (CONSUME_KW(TK_INT)) {
		return true;
	}
	return false;
}

[[noreturn]] static void throwUnsupportedAttribute(const string &name, const CToken *at)
{
	throw CPreprocessError(PlnC2AstMessage::getMessage(E_UnsupportedAttribute, name),
		at->lexer_no, at->token0_no);
}

// For attrs recorded by attribute_specifiers() with an "unsupported" entry.
[[noreturn]] static void throwUnsupportedAttribute(const json &attrs)
{
	const json &u = attrs["unsupported"];
	throw CPreprocessError(PlnC2AstMessage::getMessage(E_UnsupportedAttribute, u["name"].get<string>()),
		u["lexer"].get<int>(), u["token0"].get<int>());
}

// Combines attributes as GCC does: alignment only grows, and the first
// unsupported attribute is the one reported.
static void mergeAttributes(json &into, const json &from)
{
	if (from.contains("packed")) into["packed"] = true;
	if (from.contains("aligned"))
		into["aligned"] = max(into.value("aligned", 0LL), from["aligned"].get<long long>());
	if (from.contains("unsupported") && !into.contains("unsupported"))
		into["unsupported"] = from["unsupported"];
}

static bool isLitInt(const json &node);
static unsigned long long litIntBits(const json &node);

bool CParser::struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_struct)
{
	int index = result_index;
	json struct_attrs = json::object();
	if (!attribute_specifiers(struct_attrs, tokens, index)) return false;

	if (CONSUME(TT_ID)) {
		// struct with tag
		ast["struct-name"] = *tokens[index-1]->info.id;
		if (!CONSUME_PUNC('{')) {
			// struct with tag only (reference, not definition)
			captureStructTag(ast["struct-name"].get<string>(), nullptr, !is_struct);
			result_index = index;
			return true;
		}
	} else {
		// anonymous struct
		EXPECT_PUNC('{');
	}

	vector<json> fields;
	do {
		// Qualifiers ("const"/"volatile"/"inline") are consumed by
		// declaration_specifiers() itself (in any order); don't pre-consume them
		// here, or its "const" capture never sees them.
		int field_start = index;
		json flocal;
		if (declaration_specifiers(flocal, tokens, index)) {
			json base_vt = flocal.value("var-type", json{});
			// "struct { ... } name;" member: without a tag the owner's layout can't
			// reference the body. Synthesized on base_vt, so pointer/array
			// declarators wrap an already-named type.
			string btk = base_vt.value("type-kind", "");
			if ((btk == "strct" || btk == "union") && !base_vt.contains("type-name")) {
				string tag = synthesizeAnonTag(tokens[field_start]);
				captureStructTag(tag, &flocal, btk == "union");
				base_vt["type-name"] = tag;
				// C11 anonymous member: no member name, its fields belong to the owner.
				if (CONSUME_PUNC(';')) {
					fields.push_back({{"var-type", base_vt}}); // LCOV_EXCL_BR_LINE
					continue;
				}
			}
			json spec_attrs = flocal.value("attributes", json::object());
			do {
				json field = {{"var-type", base_vt}, {"attributes", spec_attrs}}; // LCOV_EXCL_BR_LINE
				int peek = index;
				bool unnamed_bits = consume_punc(':', tokens, peek);
				if (!unnamed_bits && !declarator(field, tokens, index, false)) {
					return false;
				}
				json attrs = move(field["attributes"]);
				field.erase("attributes");
				if (CONSUME_PUNC(':')) {
					json width;
					if (!constant_expression(width, tokens, index)) return false;
					// A width that doesn't fold leaves the struct's layout unknown.
					field["bits"] = isLitInt(width) ? json(litIntBits(width)) : json(); // LCOV_EXCL_BR_LINE
					if (!attribute_specifiers(attrs, tokens, index)) return false;
				}
				if (attrs.contains("unsupported")) throwUnsupportedAttribute(attrs);
				for (const char *key : {"packed", "aligned"})
					if (attrs.contains(key)) field[key] = attrs[key];
				fields.push_back(move(field));
			} while (CONSUME_PUNC(','));
			EXPECT_PUNC(';');
		} else {
			break;
		}
	} while (true);

	EXPECT_PUNC('}');
	if (!attribute_specifiers(struct_attrs, tokens, index)) return false;
	if (struct_attrs.contains("unsupported")) throwUnsupportedAttribute(struct_attrs);

	ast["fields"] = move(fields);
	for (const char *key : {"packed", "aligned"})
		if (struct_attrs.contains(key)) ast[key] = struct_attrs[key];
	string tagName = ast.value("struct-name", "");
	if (!tagName.empty())
		captureStructTag(tagName, &ast, !is_struct);
	result_index = index;
	return true;
} // LCOV_EXCL_BR_LINE

bool CParser::enum_definition(string &name, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	bool anonymous = !CONSUME(TT_ID);
	if (!anonymous) {
		name = *tokens[index-1]->info.id;
		if (!CONSUME_PUNC('{')) {
			// enum with tag only
			result_index = index;
			return true;
		}
	} else {
		if (index >= tokens.size()) return false;
		name = synthesizeAnonTag(tokens[index]);
		EXPECT_PUNC('{');
	}

	// A redefinition of a tag is a C error; the first body stays.
	bool capture = !enumIndex_.count(name);
	int enumIndex = (int)capturedEnums_.size();
	CapturedEnum captured = {name, anonymous, true, {}};
	long long next = 0;
	bool nextKnown = true;
	do {
		if (!CONSUME(TT_ID)) break;
		string ename = *tokens[index-1]->info.id;
		long long v = next;
		bool known = nextKnown;
		if (CONSUME_PUNC('=')) {
			json enum_value;
			if (!constant_expression(enum_value, tokens, index)) {
				return false;
			}
			known = enum_value.is_object() && enum_value.value("expr-type", "") == "lit-int";
			if (known) {
				try { v = stoll(enum_value["value"].get<string>()); }
				catch (const out_of_range&) { known = false; }
			}
		}
		if (!known) {
			captured.evaluable = false;
		} else if (capture) {
			captured.values.push_back({ename, v});
			enumerators_[ename] = {v, enumIndex};
		}
		nextKnown = known && v != LLONG_MAX;
		if (nextKnown) next = v + 1;

		if (!CONSUME_PUNC(',')) {
			break;
		}

	} while (true);

	EXPECT_PUNC('}');
	if (capture) {
		if (!captured.evaluable)
			for (auto &[ename, v] : captured.values) enumerators_.erase(ename);
		enumIndex_[name] = enumIndex;
		capturedEnums_.push_back(move(captured));
	}
	result_index = index;
	return true;
}

// "typedef enum { ... } Name;" names the anonymous body after the typedef, as
// declaration() does for an anonymous struct, unless the name is already a
// type's.
void CParser::nameAnonEnumByTypedef(json &vt, const string &typedefName)
{
	auto it = enumIndex_.find(vt["type-name"].get<string>());
	if (it == enumIndex_.end()) return;
	CapturedEnum &e = capturedEnums_[it->second];
	if (!e.anonymous || enumIndex_.count(typedefName) || structIndex_.count(typedefName)) return;
	int idx = it->second;
	enumIndex_.erase(it);
	enumIndex_[typedefName] = idx;
	e.name = typedefName;
	e.anonymous = false;
	vt["type-name"] = typedefName;
}

// Qualifiers and `_Complex` may come before or after the type specifier, in any
// order ("const volatile int", "char const *", "double _Complex").
bool CParser::declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	bool is_const = false, is_complex = false;
	auto qualifier = [&]() {
		if (CONSUME_KW(TK_INLINE) || CONSUME_KW(TK_VOLATILE)) return true;
		if (CONSUME_KW(TK_CONST)) return is_const = true;
		if (CONSUME_KW(TK_COMPLEX)) return is_complex = true;
		return false;
	};
	json attrs = json::object();
	for (;;) {
		if (qualifier()) continue;
		int before = index;
		if (!attribute_specifiers(attrs, tokens, index)) return false;
		if (index != before) continue;
		break;
	}
	if (!attrs.empty()) ast["attributes"] = attrs;

	if (!type_specifier(ast, tokens, index)) return false;
	while (qualifier())
		;
	json &vt = ast["var-type"];
	if (is_complex) {
		static const map<string, string> complexNames = {
			{"flo32", "float _Complex"}, {"flo64", "double _Complex"}, {"flt128", "long double _Complex"},
		}; // LCOV_EXCL_BR_LINE
		auto it = vt.value("type-kind", "") == "prim" ? complexNames.find(vt["type-name"].get<string>())
			: complexNames.end();
		if (it == complexNames.end()) return false;
		vt["type-name"] = it->second;
	}
	if (is_const) vt["const"] = true;
	result_index = index;
	return true;
} // LCOV_EXCL_BR_LINE

bool CParser::type_specifier(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	auto set_type = [&](json vt) { ast["var-type"] = move(vt); };
	auto set_prim = [&](const char* name) {
		set_type({{"type-kind", "prim"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
	}; // LCOV_EXCL_BR_LINE

	if (CONSUME(TT_ID)) {	// typedef name
		string name = *tokens[index-1]->info.id;
		auto it = typedefs_.find(name);
		if (it != typedefs_.end()) {
			json vt = it->second;
			vt["typedef-name"] = name;
			set_type(move(vt));
		} else {
			set_type({{"type-kind", "user"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
		}
		result_index = index;
		return true;
	}

	{
		bool is_unsigned = CONSUME_KW(TK_UNSIGNED);
		if (!is_unsigned) CONSUME_KW(TK_SIGNED);
		if (CONSUME_KW(TK_INT128)) {
			set_prim(is_unsigned ? "unsigned __int128" : "__int128");
			result_index = index;
			return true;
		}
		index = result_index;
	}
	if (unsigned_char(tokens, index)) { set_prim("uint8"); result_index = index; return true; }
	if (signed_char(tokens, index))   { set_prim("int8");  result_index = index; return true; }
	if (unsigned_long_long(tokens, index)) { set_prim("uint64"); result_index = index; return true; }
	if (signed_long_long(tokens, index))   { set_prim("int64");  result_index = index; return true; }

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_DOUBLE)) { set_prim("flt128"); result_index = index; return true; }
		index--; // backtrack
	}

	if (unsigned_long(tokens, index))  { set_prim("uint64"); result_index = index; return true; }
	if (signed_long(tokens, index))    { set_prim("int64");  result_index = index; return true; }
	if (unsigned_short(tokens, index)) { set_prim("uint16"); result_index = index; return true; }
	if (signed_short(tokens, index))   { set_prim("int16");  result_index = index; return true; }
	if (unsigned_int(tokens, index))   { set_prim("uint32"); result_index = index; return true; }
	if (signed_int(tokens, index))     { set_prim("int32");  result_index = index; return true; }

	if (CONSUME_KW(TK_DOUBLE)) { set_prim("flo64"); result_index = index; return true; }
	if (CONSUME_KW(TK_FLOAT))  { set_prim("flo32"); result_index = index; return true; }

	if (CONSUME_KW(TK_STRUCT)) {
		if (struct_union_definition(ast, tokens, index, true)) {
			json vt = {{"type-kind", "strct"}}; // LCOV_EXCL_BR_LINE
			string tagName = ast.value("struct-name", "");
			if (!tagName.empty()) {
				vt["type-name"] = tagName;
			}
			set_type(move(vt));
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_UNION)) {
		if (struct_union_definition(ast, tokens, index, false)) {
			json vt = {{"type-kind", "union"}}; // LCOV_EXCL_BR_LINE
			string tagName = ast.value("struct-name", "");
			if (!tagName.empty()) {
				vt["type-name"] = tagName;
			}
			set_type(move(vt));
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_ENUM)) {
		string name;
		if (enum_definition(name, tokens, index)) {
			// Every enum is laid out as int; a packed or aligned one would not be.
			json enum_attrs = json::object();
			int attr_start = index;
			if (!attribute_specifiers(enum_attrs, tokens, index)) return false;
			if (enum_attrs.contains("unsupported")) throwUnsupportedAttribute(enum_attrs);
			if (!enum_attrs.empty())
				throwUnsupportedAttribute(enum_attrs.contains("packed") ? "packed" : "aligned",
					tokens[attr_start]);
			set_type({{"type-kind", "enum"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_VOID)) { set_prim("void"); result_index = index; return true; }
	if (CONSUME_KW(TK_BOOL)) { set_prim("bool"); result_index = index; return true; }

	return false;
} // LCOV_EXCL_BR_LINE

// C array-parameter decay: "T name[n]...[m]" as a parameter type means "pointer to
// T[m]..." -- only the outermost dimension decays to a pointer, any inner dimensions
// stay as the pointee's array type (e.g. "char buf[2][3]" -> pntr(arr(size=3, ...))).
static void decayArrayParam(json &param)
{
	json &vt = param["var-type"];
	if (vt.value("type-kind", "") == "arr") {
		vt = {{"type-kind", "pntr"}, {"base-type", vt["base-type"]}}; // LCOV_EXCL_BR_LINE
	}
} // LCOV_EXCL_BR_LINE

// A parameter's layout attributes don't change how it is passed; one that
// changes its type can't be represented.
static void takeParamAttributes(json &param)
{
	if (param["attributes"].contains("unsupported")) throwUnsupportedAttribute(param["attributes"]);
	param.erase("attributes");
}

bool CParser::parameter_list(vector<json> &params, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	json local;
	if (declaration_specifiers(local, tokens, index)) {
		json param = {{"var-type", local.value("var-type", json{})},
			{"attributes", local.value("attributes", json::object())}}; // LCOV_EXCL_BR_LINE
		if (!declarator(param, tokens, index, false)) {
			if (!declarator(param, tokens, index, true)) { // abstract declarator
				// debug_token(tokens[index]);
				return false;
			}
		}
		takeParamAttributes(param);
		decayArrayParam(param);
		params.push_back(param);

		while (CONSUME_PUNC(',')) {
			json local2;
			if (declaration_specifiers(local2, tokens, index)) {
				json param2 = {{"var-type", local2.value("var-type", json{})},
					{"attributes", local2.value("attributes", json::object())}}; // LCOV_EXCL_BR_LINE
				if (!declarator(param2, tokens, index, false)) {
					if (!declarator(param2, tokens, index, true)) { // abstract declarator
						// debug_token(tokens[index]);
						return false;
					}
				}
				takeParamAttributes(param2);
				decayArrayParam(param2);
				params.push_back(param2);
			} else if (CONSUME_PUNC('...')) {
				params.push_back({{"name", "..."}}); // LCOV_EXCL_BR_LINE
				EXPECT_PUNC(')');
				index--;	// backtrack for ')'
			} else {
				// debug_token(tokens[index]);
				return false;
			}
		}

		// C's `(void)` prototype means "no parameters" -- normalize it to the
		// same empty list a `f()` prototype and a Palan 0-arg function already
		// carry (ASTSpec.md: "empty array when no parameters"), so no
		// consumer needs to know about this pseudo-parameter shape. Applies
		// equally to a function-pointer's own inner `(void)` parameter list,
		// since that goes through this same function.
		if (params.size() == 1 && !params[0].contains("name")
				&& params[0]["var-type"].value("type-kind", "") == "prim"
				&& params[0]["var-type"].value("type-name", "") == "void")
			params.clear();

		result_index = index;
		return true;
	}
	return false;
} // LCOV_EXCL_BR_LINE

// Wraps decl["var-type"] with pending array dimensions (outermost-first in `dims`),
// nesting from the innermost (last-parsed) dimension outward so "T m[2][3]" becomes
// arr(size=2, base=arr(size=3, base=T)) -- matching C array-of-array semantics.
// embedded:true / specifier:"raw" mark this as inline storage (no separate heap
// allocation), matching Palan's native [n]$T field vocabulary (ASTSpec.md "arr").
static void wrapArrayDims(json &var_type, vector<json> &dims)
{
	for (int i = (int)dims.size() - 1; i >= 0; --i) {
		var_type = {{"type-kind", "arr"}, {"base-type", var_type},
			{"size-expr", dims[i]}, {"embedded", true}, {"specifier", "raw"}}; // LCOV_EXCL_BR_LINE
	}
	dims.clear();
} // LCOV_EXCL_BR_LINE

bool CParser::declarator_tail(json &decl, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	vector<json> dims;   // pending array dimensions, outermost (leftmost) first

	while (true) {
		if (CONSUME_PUNC('[')) {
			// A parameter's "[restrict]" or "[static n]" qualifies only the
			// pointer the callee receives.
			while (CONSUME_KW(TK_CONST) || CONSUME_KW(TK_VOLATILE) || CONSUME_KW(TK_RESTRICT)
					|| CONSUME_KW(TK_STATIC))
				;
			json arr_size_value;
			if (!CONSUME_PUNC(']')) {
				constant_expression(arr_size_value, tokens, index);
				if (!CONSUME_PUNC(']')) {
					// debug_token(tokens[index]);
					return false;
				}
				// A null size-expr means `[]`, so a size that doesn't fold must not
				// reach the AST as one.
				if (arr_size_value.is_null()) arr_size_value = {{"expr-type", "not-const"}}; // LCOV_EXCL_BR_LINE
			}
			dims.push_back(move(arr_size_value));

		} else if (CONSUME_PUNC('(')) {
			wrapArrayDims(decl["var-type"], dims);
			vector<json> params;
			if (!CONSUME_PUNC(')')) {
				parameter_list(params, tokens, index);
				EXPECT_PUNC(')');
			}
			decl["var-type"] = {{"type-kind", "func"}, {"ret-type", decl["var-type"]}, {"parameters", params}}; // LCOV_EXCL_BR_LINE

		} else {
			break;
		}
	}
	wrapArrayDims(decl["var-type"], dims);

	result_index = index;
	return true;
} // LCOV_EXCL_BR_LINE

// Advances `index` from the token just after an already-consumed `open` to the
// position just after its matching `close`, tracking nested pairs (a parameter list
// inside a group, e.g. "(*signal(int))(int)", may itself contain '(' / ')'). Sets
// `close_index` to the token index of that matching `close` itself, which declarator()
// uses to confirm a grouped declarator's inner parse consumed exactly up to it (see
// declarator() below).
static bool skipToMatching(const vector<CToken*> &tokens, int &index, int &close_index,
		int open = '(', int close = ')')
{
	int depth = 1;
	while (index < (int)tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_PUNCTUATOR) {
			if (token->info.punc == open) {
				depth++;
			} else if (token->info.punc == close) {
				depth--;
				if (depth == 0) {
					close_index = index;
					index++;
					return true;
				}
			}
		}
		index++;
	}
	return false;
}

// declarator() parses a declarator, building up decl:
//   decl["name"]     - declared identifier
//   decl["var-type"] - complete type (caller initializes with base type from declaration_specifiers)
//
// C declarator precedence: the postfix suffixes '[n]' and '(params)' bind tighter than
// the prefix '*', so prefix pointers at this level are applied to the base type first
// (innermost), suffixes then wrap outside them, and a parenthesized inner declarator
// (a new level) receives the type built so far as ITS base type -- e.g. "int (*a)[3]"
// wraps int in arr(3) first (the suffix, at this level), then the inner "*a" wraps
// that in pntr (one level in); "int *a[3]" has no group, so the prefix pntr wraps int
// directly and the suffix arr(3) wraps outside that, giving arr(3, pntr(int)).
//
// This mutates decl["var-type"] before knowing whether the parse will succeed (the
// prefix '*' loop below writes it immediately), so any failure must roll back to
// `saved` -- callers such as parameter_list() retry a failed declarator() call on the
// very same decl object (first as a named declarator, then as an abstract one), and
// would otherwise see a partially-wrapped type left over from the failed attempt.
bool CParser::declarator(json &decl, const vector<CToken*> &tokens, int &result_index, bool is_typeonly)
{
	int index = result_index;
	json saved = decl;
	json attrs = decl.value("attributes", json::object());
	auto fail = [&]() { decl = move(saved); return false; };

	if (!attribute_specifiers(attrs, tokens, index)) return fail();
	while (CONSUME_PUNC('*')) {
		bool ptr_const = false;
		for (;;) {
			if (CONSUME_KW(TK_CONST)) ptr_const = true;
			else if (!CONSUME_KW(TK_VOLATILE) && !CONSUME_KW(TK_RESTRICT)) break;
		}
		decl["var-type"] = {{"type-kind", "pntr"}, {"base-type", move(decl["var-type"])}}; // LCOV_EXCL_BR_LINE
		if (ptr_const) decl["var-type"]["const"] = true;
		if (!attribute_specifiers(attrs, tokens, index)) return fail();
	}

	int group_index = -1, close_index = -1;
	if (CONSUME_PUNC('(')) {
		group_index = index;
		if (!skipToMatching(tokens, index, close_index)) return fail();

	} else if (!is_typeonly) {
		if (!CONSUME(TT_ID)) return fail();
		decl["name"] = *tokens[index-1]->info.id;
	}

	if (!declarator_tail(decl, tokens, index)) return fail();
	if (!attribute_specifiers(attrs, tokens, index)) return fail();
	if (!attrs.empty()) decl["attributes"] = move(attrs);

	if (group_index >= 0) {
		// Parse the group's interior as a nested declarator level, now that the type
		// it should wrap (this level's suffixes, applied above) is in decl["var-type"].
		// Requiring it to consume exactly up to the already-located close_index (rather
		// than just trusting its own success) rejects garbage inside the group, e.g.
		// "int (*x y)" -- and rejects a parameter list mistaken for a group, e.g.
		// "int(char*)", the same shapes the old EXPECT_PUNC(')')-right-after-recursing
		// check used to reject.
		int gi = group_index;
		if (!declarator(decl, tokens, gi, is_typeonly) || gi != close_index) return fail();
	}

	result_index = index;
	return true;
} // LCOV_EXCL_BR_LINE

bool CParser::declaration(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_top_level)
{
	int index = result_index;

	bool is_extern = false;
	bool is_static = false;
	json decl_attrs = json::object();
	if (!attribute_specifiers(decl_attrs, tokens, index)) return false;
	bool is_typedef = CONSUME_KW(TK_TYPEDEF);

	if (!is_typedef) {
		is_extern = CONSUME_KW(TK_EXTERN);
		if (!is_extern) {
			is_static = CONSUME_KW(TK_STATIC);
		}
	}

	if (!(is_typedef || is_extern || is_static)) {
		// just declaration of struct or union
		int struct_union_save_index = index;
		bool is_struct_kw = CONSUME_KW(TK_STRUCT);
		bool is_union_kw = !is_struct_kw && CONSUME_KW(TK_UNION);
		if (is_struct_kw || is_union_kw) {
			// Capturing into capturedStructs_ happens inside struct_union_definition()
			// itself (the single point every struct tag reference goes through);
			// nothing to do here beyond clearing the scratch keys it wrote into ast.
			if (struct_union_definition(ast, tokens, index, is_struct_kw) && CONSUME_PUNC(';')) {
				for (const char *key : {"struct-name", "fields", "packed", "aligned"})
					ast.erase(key);
				result_index = index;
				return true;
			}
			// Not a standalone struct/union declaration (e.g. "struct Tag func(...)")
			// — backtrack and let it fall through to be parsed as a type specifier.
			index = struct_union_save_index;
		}
	}

	json local;
	if (declaration_specifiers(local, tokens, index)) {
		json base_vt = local.value("var-type", json{});
		// Just a declaration of an enum. Recognized after the specifier rather
		// than ahead of it, so "enum Tag f(void);" needs no backtrack that
		// would capture the body twice.
		if (base_vt.value("type-kind", "") == "enum" && CONSUME_PUNC(';')) {
			result_index = index;
			return true;
		}
		if (local.contains("attributes")) mergeAttributes(decl_attrs, local["attributes"]);
		vector<json> decls;
		decls.push_back({{"var-type", base_vt}, {"attributes", decl_attrs}}); // LCOV_EXCL_BR_LINE
		if (declarator(decls.back(), tokens, index, false)) {
			// parsed first declarator

			// Comma-separated additional declarators ("extern int a, b;",
			// "typedef int A, B;") are classified the same way as the first,
			// below -- all go through emitDeclarator() rather than only the
			// first being visible.
			while (CONSUME_PUNC(',')) {
				decls.push_back({{"var-type", base_vt}, {"attributes", decl_attrs}}); // LCOV_EXCL_BR_LINE
				bool ok = declarator(decls.back(), tokens, index, false);
				BOOST_ASSERT(ok);
			}

			if (CONSUME_PUNC(';')) {
				// simple declaration(s)

				// "typedef struct/union { ... } Name;" -- an anonymous body given a
				// name only through the typedef, not a tag. Synthesize that name as
				// the tag so the body enters capturedStructs_/typedefs_ through the
				// same single channel a tagged one would, rather than staying
				// unresolved as "user" at every reference site. Guarded to a single,
				// non-derived declarator (decls.size()==1, var-type still bare "strct"/"union"
				// after declarator() -- a pointer/array declarator would have wrapped
				// it in "pntr") whose var-type has no type-name yet (tagged structs and
				// typedef-name lookups of an already-registered strct always carry one).
				// Skipped outright if the name is already taken by a real C tag
				// (forward-declared or defined): Palan's struct type names are a single
				// namespace (unlike C's separate tag/typedef namespaces), so a name
				// already bound to one struct cannot also be re-bound to this unrelated
				// anonymous one -- doing so would let captureStructTag's promote-in-place
				// behavior attach this body's fields to that other struct's entry.
				if (is_typedef && decls.size() == 1 && local.contains("fields")) {
					json& vt0 = decls[0]["var-type"];
					string tk0 = vt0.value("type-kind", "");
					if ((tk0 == "strct" || tk0 == "union") && !vt0.contains("type-name")) {
						const string& name = decls[0]["name"].get<string>();
						// GCC gives a typedef's own aligned attribute to the name
						// only (the body keeps its size), which Palan can't
						// express; such a typedef stays unregistered.
						if (structIndex_.find(name) == structIndex_.end()
								&& decls[0]["attributes"].empty()) {
							captureStructTag(name, &local, tk0 == "union");
							vt0["type-name"] = name;
						}
					}
				}

				if (is_typedef && decls.size() == 1 && decls[0]["var-type"].value("type-kind", "") == "enum")
					nameAnonEnumByTypedef(decls[0]["var-type"], decls[0]["name"].get<string>());

				for (auto &d : decls) {
					emitDeclarator(ast, d, is_typedef, is_static, is_extern, is_top_level);
				}
				result_index = index;
				return true;

			} else if (is_top_level && decls.size() == 1
					&& decls[0]["var-type"].value("type-kind", "") == "func" && CONSUME_PUNC('{')) {
				// A function definition is a declaration: its body (inline asm,
				// statements) is nothing Palan calls into.
				int close_index;
				if (!skipToMatching(tokens, index, close_index, '{', '}')) return false;
				emitDeclarator(ast, decls[0], is_typedef, is_static, is_extern, is_top_level);
				result_index = index;
				return true;
			}
			return false;
		}
	}

	return false;

} // LCOV_EXCL_BR_LINE

static bool isReservedName(const string& name)
{
	return name.size() >= 2 && name[0] == '_' && (name[1] == '_' || isupper((unsigned char)name[1]));
}

// Classifies one completed declarator (the first in a declaration, or any
// comma-separated successor) and emits it into the appropriate AST channel,
// or registers it into typedefs_, or discards it. This is the single point
// every declarator passes through, so multi-declarator lists are classified
// uniformly instead of only the first declarator being visible to callers.
void CParser::emitDeclarator(json &ast, json &decl,
		bool is_typedef, bool is_static, bool is_extern, bool is_top_level)
{
	auto& vt = decl["var-type"];
	BOOST_ASSERT(vt.is_object());
	string tk = vt.value("type-kind", "");
	json attrs = move(decl["attributes"]);
	decl.erase("attributes");
	if (!attrs.empty()) {
		// A typedef can't carry its own type change or alignment into Palan, so
		// the name stays unknown; elsewhere only a type change matters.
		if (is_typedef) return;
		if (attrs.contains("unsupported")) throwUnsupportedAttribute(attrs);
	}

	if (!is_static && !is_typedef && tk == "func") {
		BOOST_ASSERT(decl.contains("name"));
		BOOST_ASSERT(vt.contains("ret-type") && !vt["ret-type"].is_null());
		// LCOV_EXCL_BR_START
		ast["ast"]["functions"].push_back({
			{"name", move(decl["name"])},
			{"func-type", "c"},
			{"ret-type", move(vt["ret-type"])},
			{"parameters", move(vt["parameters"])}
		});
		// LCOV_EXCL_BR_STOP
	} else if (is_typedef) {
		if (tk == "prim" || tk == "pntr" || tk == "func") {
			registerTypedef(decl["name"].get<string>(), vt);
		} else if ((tk == "strct" || tk == "union" || tk == "enum") && vt.contains("type-name")) {
			// typedef struct/union/enum Tag X; -- register X as an alias for the tag
			// itself (SA resolves it the same way it resolves any other
			// struct-bottomed type alias). This also covers a single,
			// non-derived typedef of an anonymous body (typedef struct/union {...}
			// X;): declaration() has already synthesized X itself as the tag
			// (see its call site) and written it into vt's "type-name" before
			// this function runs, so that case and the tagged one are
			// indistinguishable here.
			const string& name = decl["name"].get<string>();
			// A message names the struct by the first typedef written over
			// `struct Tag` itself, since Palan code uses that name. A C reserved
			// name gives way to a later one: glibc declares `__FILE` before `FILE`.
			auto it = structIndex_.find(vt["type-name"].get<string>());
			if (it != structIndex_.end() && !vt.contains("typedef-name") && name != it->first) {
				json& entry = capturedStructs_[it->second];
				if (!entry.contains("typedef-name")
						|| (isReservedName(entry["typedef-name"].get<string>()) && !isReservedName(name)))
					entry["typedef-name"] = name;
			}
			registerTypedef(name, vt);
		} else if (tk == "user") {
			// An unresolved name may be an earlier cinclude's typedef, which SA
			// resolves; registering keeps this typedef's references pointing at it.
			registerTypedef(decl["name"].get<string>(), vt);
		}
		// Remaining anonymous strct/union underlying types -- a
		// multi/derived-declarator anonymous struct/union body, or a name
		// declaration()'s tag synthesis skipped because it was already taken
		// by an unrelated struct -- are not registered, left as unresolved
		// "user" at reference sites (unchanged behavior).
	} else if (is_extern && is_top_level && (tk == "prim" || tk == "pntr" || tk == "enum")) {
		// A file-scope "extern" object declaration with external linkage, of a
		// type Palan can represent without heap/embedded-array semantics
		// (e.g. "extern FILE *stdout;"). Array and by-value struct/union
		// globals are left unregistered -- same non-goal as elsewhere in this
		// iteration.
		// LCOV_EXCL_BR_START
		ast["ast"]["globals"].push_back({
			{"name", move(decl["name"])},
			{"var-type", move(vt)}
		});
		// LCOV_EXCL_BR_STOP
	}
	// else: static function, non-func/non-typedef/non-extern-object
	// declaration, or a shape outside what the globals channel represents --
	// discarded, matching pre-existing behavior for everything but
	// "func"/typedef.
} // LCOV_EXCL_BR_LINE

void CParser::registerTypedef(const string &name, json vt)
{
	vt.erase("typedef-name");
	typedefs_[name] = move(vt);
}

// A value-AST "lit-int" follows the Palan AST convention for "value-type": present only
// when C fixed the type (a suffixed literal, an integer cast, or an operation with such an
// operand). An untyped node is a plain unsuffixed literal and folds with exact int64
// arithmetic. The value string is always within the range of its value-type.

static bool isLitInt(const json &node)
{
	return node.is_object() && node.value("expr-type", "") == "lit-int";
}

static bool intTypeInfo(const json &type, int &width, bool &is_signed)
{
	if (!type.is_object() || type.value("type-kind", "") != "prim") return false;
	static const map<string, pair<int, bool>> infos = {
		{"int8", {8, true}}, {"int16", {16, true}}, {"int32", {32, true}}, {"int64", {64, true}},
		{"uint8", {8, false}}, {"uint16", {16, false}}, {"uint32", {32, false}}, {"uint64", {64, false}},
	}; // LCOV_EXCL_BR_LINE
	auto it = infos.find(type.value("type-name", ""));
	if (it == infos.end()) return false;
	width = it->second.first;
	is_signed = it->second.second;
	return true;
} // LCOV_EXCL_BR_LINE

static json intPrimType(int width, bool is_signed)
{
	return {{"type-kind", "prim"}, {"type-name", (is_signed ? "int" : "uint") + to_string(width)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// Two's complement bit pattern of a lit-int's value.
static unsigned long long litIntBits(const json &node)
{
	const string &s = node["value"].get_ref<const string&>();
	return s[0] == '-' ? (unsigned long long)stoll(s) : stoull(s);
}

// Builds a lit-int of `type` from `bits`, wrapping to the type's width the way GCC
// converts between integer types.
static json makeTypedLitInt(unsigned long long bits, const json &type)
{
	int width;
	bool is_signed;
	intTypeInfo(type, width, is_signed);
	if (width < 64) {
		unsigned long long mask = (1ULL << width) - 1;
		bits &= mask;
		if (is_signed && (bits >> (width - 1)))
			bits |= ~mask;
	}
	string value = is_signed ? to_string((long long)bits) : to_string(bits);
	return {{"expr-type", "lit-int"}, {"value", value}, {"value-type", type}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// C11 6.4.4.1 integer constant typing (LP64). An unsuffixed literal stays untyped.
static json parseIntLiteral(const string &text)
{
	size_t consumed;
	unsigned long long v;
	try {
		v = stoull(text, &consumed, 0);
	} catch (...) {
		return json{};
	}

	string suffix;
	for (size_t i = consumed; i < text.size(); i++) suffix += tolower(text[i]);
	bool has_u = false;
	int l_count = 0;
	if (!suffix.empty() && suffix.front() == 'u') { has_u = true; suffix.erase(0, 1); }
	if (!suffix.empty() && suffix.back() == 'u' && !has_u) { has_u = true; suffix.pop_back(); }
	if (suffix == "l") l_count = 1;
	else if (suffix == "ll") l_count = 2;
	else if (!suffix.empty()) return json{}; // float literal or invalid suffix

	bool is_decimal = text.size() == 1 || text[0] != '0';
	if (!has_u && !l_count) {
		if (v > LLONG_MAX) return json{};
		return {{"expr-type", "lit-int"}, {"value", to_string(v)}}; // LCOV_EXCL_BR_LINE
	}
	if (has_u) {
		bool fits32 = !l_count && v <= UINT32_MAX;
		return makeTypedLitInt(v, intPrimType(fits32 ? 32 : 64, false));
	}
	if (v > LLONG_MAX && is_decimal) return json{};
	return makeTypedLitInt(v, intPrimType(64, v <= LLONG_MAX));
} // LCOV_EXCL_BR_LINE

// Width and signedness a lit-int operand takes in C arithmetic, after integer promotion.
// An untyped operand is taken as int, or long when its value doesn't fit int.
static void promotedIntType(const json &node, int &width, bool &is_signed)
{
	if (node.contains("value-type")) {
		intTypeInfo(node["value-type"], width, is_signed);
		if (width < 32) { width = 32; is_signed = true; }
		return;
	}
	long long v = stoll(node["value"].get<string>());
	width = (v >= INT32_MIN && v <= INT32_MAX) ? 32 : 64;
	is_signed = true;
}

static bool fitsSigned(long long v, int width)
{
	return width == 64 || (v >= INT32_MIN && v <= INT32_MAX);
}

static json foldUntypedBinaryInt(long long l, int op, long long r)
{
	long long v;
	switch (op) {
	case '|': v = l | r; break;
	case '&': v = l & r; break;
	case '^': v = l ^ r; break;
	case '+': if (__builtin_add_overflow(l, r, &v)) return json{}; break;
	case '-': if (__builtin_sub_overflow(l, r, &v)) return json{}; break;
	case '*': if (__builtin_mul_overflow(l, r, &v)) return json{}; break;
	case '/':
		if (r == 0 || (l == LLONG_MIN && r == -1)) return json{};
		v = l / r;
		break;
	case '%':
		if (r == 0 || (l == LLONG_MIN && r == -1)) return json{};
		v = l % r;
		break;
	case '<<':
		if (l < 0 || r < 0 || r >= 63 || l > (LLONG_MAX >> r)) return json{};
		v = l << r;
		break;
	case '>>':
		if (l < 0 || r < 0 || r >= 64) return json{};
		v = l >> r;
		break;
	default:
		return json{};
	}
	return {{"expr-type", "lit-int"}, {"value", to_string(v)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// Shift: the result takes the promoted left operand's type (not the usual arithmetic
// conversions), and a count outside [0, width) is undefined behavior.
static json foldTypedShift(const json &lhs, int op, const json &rhs)
{
	int width, r_width;
	bool is_signed, r_signed;
	promotedIntType(lhs, width, is_signed);
	promotedIntType(rhs, r_width, r_signed);
	unsigned long long l = litIntBits(lhs), r = litIntBits(rhs);
	if ((r_signed && (long long)r < 0) || r >= (unsigned long long)width) return json{};

	json type = intPrimType(width, is_signed);
	if (is_signed) {
		long long sl = (long long)l;
		long long max = width == 64 ? LLONG_MAX : INT32_MAX;
		if (sl < 0 || (op == '<<' && sl > (max >> r))) return json{};
	}
	return makeTypedLitInt(op == '<<' ? l << r : l >> r, type);
}

// Folds with C's usual arithmetic conversions: unsigned results wrap to their width,
// signed results that overflow their width don't fold.
static json foldTypedBinaryInt(const json &lhs, int op, const json &rhs)
{
	if (op == '<<' || op == '>>') return foldTypedShift(lhs, op, rhs);

	int l_width, r_width;
	bool l_signed, r_signed;
	promotedIntType(lhs, l_width, l_signed);
	promotedIntType(rhs, r_width, r_signed);
	int width = max(l_width, r_width);
	bool is_signed = !((l_width == width && !l_signed) || (r_width == width && !r_signed));
	json type = intPrimType(width, is_signed);
	unsigned long long l = litIntBits(lhs), r = litIntBits(rhs);

	if (!is_signed) {
		unsigned long long mask = width == 64 ? ~0ULL : (1ULL << width) - 1;
		l &= mask;
		r &= mask;
		unsigned long long v;
		switch (op) {
		case '|': v = l | r; break;
		case '&': v = l & r; break;
		case '^': v = l ^ r; break;
		case '+': v = l + r; break;
		case '-': v = l - r; break;
		case '*': v = l * r; break;
		case '/': if (r == 0) return json{}; v = l / r; break;
		case '%': if (r == 0) return json{}; v = l % r; break;
		default: return json{};
		}
		return makeTypedLitInt(v, type);
	}

	json folded = foldUntypedBinaryInt((long long)l, op, (long long)r);
	if (!isLitInt(folded)) return json{};
	long long v = stoll(folded["value"].get<string>());
	if (!fitsSigned(v, width)) return json{};
	return makeTypedLitInt((unsigned long long)v, type);
}

// Folds one binary C operation on two value-AST nodes into a new "lit-int" node.
// Returns null -- the expression chain's single "not evaluable" signal -- when either
// operand is null (not a lit-int), when the operator isn't one of the folded arithmetic
// or bitwise operators (relational/equality are recognized syntactically by their
// caller but intentionally not folded), or when evaluating would be undefined behavior
// in this arithmetic itself (div/mod by zero, out-of-range shift count, overflow).
// Folding these to null rather than a wrong value keeps them in the same "not a
// compile-time constant we track" vocabulary as an unresolved identifier.
static json foldBinaryInt(const json &lhs, int op, const json &rhs)
{
	if (!isLitInt(lhs) || !isLitInt(rhs)) return json{};
	if (lhs.contains("value-type") || rhs.contains("value-type"))
		return foldTypedBinaryInt(lhs, op, rhs);
	return foldUntypedBinaryInt(stoll(lhs["value"].get<string>()), op,
		stoll(rhs["value"].get<string>()));
}

static json foldNegate(const json &node)
{
	if (!isLitInt(node)) return json{};
	if (!node.contains("value-type")) {
		long long v = stoll(node["value"].get<string>());
		if (v == LLONG_MIN) return json{};
		return {{"expr-type", "lit-int"}, {"value", to_string(-v)}}; // LCOV_EXCL_BR_LINE
	}
	int width;
	bool is_signed;
	promotedIntType(node, width, is_signed);
	unsigned long long bits = litIntBits(node);
	if (is_signed) {
		long long v = (long long)bits;
		if (v == LLONG_MIN || !fitsSigned(-v, width)) return json{};
	}
	return makeTypedLitInt(0 - bits, intPrimType(width, is_signed));
} // LCOV_EXCL_BR_LINE

// An untyped operand is int or long, both signed, so ~v keeps its width and fits as is.
static json foldBitNot(const json &node)
{
	if (!isLitInt(node)) return json{};
	if (!node.contains("value-type"))
		return {{"expr-type", "lit-int"}, {"value", to_string(~stoll(node["value"].get<string>()))}}; // LCOV_EXCL_BR_LINE
	int width;
	bool is_signed;
	promotedIntType(node, width, is_signed);
	return makeTypedLitInt(~litIntBits(node), intPrimType(width, is_signed));
} // LCOV_EXCL_BR_LINE

// GCC's __attribute__((...)) lists. Only what changes a layout ("packed",
// "aligned") or a type itself ("vector_size", "mode") is recorded into
// `attrs`; the rest (noreturn, format, nonnull, ...) is skipped.
bool CParser::attribute_specifiers(json &attrs, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	while (CONSUME_KW(TK_ATTRIBUTE)) {
		EXPECT_PUNC('(');
		EXPECT_PUNC('(');
		while (!CONSUME_PUNC(')')) {
			if (index >= tokens.size()) return false;
			const CToken *at = tokens[index];
			string name;
			if (at->type == TT_ID) name = *at->info.id;
			else if (at->type == TT_KEYWORD) name = CLexer::keywords[at->info.keyword];
			else return false;
			index++;
			if (name.size() > 4 && name.starts_with("__") && name.ends_with("__"))
				name = name.substr(2, name.size() - 4);

			json found = json::object();
			auto unsupported = [&]() {
				found["unsupported"] = {{"name", name}, {"lexer", at->lexer_no}, {"token0", at->token0_no}}; // LCOV_EXCL_BR_LINE
			}; // LCOV_EXCL_BR_LINE
			if (name == "packed") found["packed"] = true;
			else if (name == "vector_size" || name == "mode") unsupported();
			if (CONSUME_PUNC('(')) {
				int args = index, close_index;
				if (!skipToMatching(tokens, index, close_index)) return false;
				if (name == "aligned") {
					json n;
					unsigned long long v = 0;
					if (constant_expression(n, tokens, args) && args == close_index && isLitInt(n)
							&& n["value"].get_ref<const string&>()[0] != '-')
						v = litIntBits(n);
					if (v && !(v & (v - 1)) && v <= LLONG_MAX) found["aligned"] = (long long)v;
					else unsupported();
				}
			} else if (name == "aligned") {
				found["aligned"] = 16LL;  // the largest alignment on x86-64
			}
			mergeAttributes(attrs, found);
			if (!CONSUME_PUNC(',')) {
				EXPECT_PUNC(')');
				break;
			}
		}
		EXPECT_PUNC(')');
	}
	result_index = index;
	return true;
}

// C's own x86-64 layout rules, not SA's buildStructDef: that one lays out only the
// shapes Palan can represent, while sizeof must size any type C allows.
bool CParser::typeSizeAlign(const json &vt, long long &size, long long &align)
{
	string tk = vt.value("type-kind", "");
	if (tk == "prim") {
		// A complex number is its two parts, aligned as one.
		static const map<string, pair<int, int>> sizes = {
			{"int8", {1, 1}}, {"uint8", {1, 1}}, {"bool", {1, 1}}, {"int16", {2, 2}}, {"uint16", {2, 2}},
			{"int32", {4, 4}}, {"uint32", {4, 4}}, {"flo32", {4, 4}}, {"int64", {8, 8}}, {"uint64", {8, 8}},
			{"flo64", {8, 8}}, {"flt128", {16, 16}},
			{"__int128", {16, 16}}, {"unsigned __int128", {16, 16}},
			{"float _Complex", {8, 4}}, {"double _Complex", {16, 8}}, {"long double _Complex", {32, 16}},
		}; // LCOV_EXCL_BR_LINE
		auto it = sizes.find(vt.value("type-name", ""));
		if (it == sizes.end()) return false;
		size = it->second.first;
		align = it->second.second;
		return true;
	}
	if (tk == "pntr") {
		size = align = 8;
		return true;
	}
	if (tk == "arr") {
		const json &n = vt["size-expr"];
		if (!isLitInt(n) || n["value"].get_ref<const string&>()[0] == '-') return false;
		unsigned long long count = stoull(n["value"].get<string>());
		long long elem_size;
		if (!typeSizeAlign(vt["base-type"], elem_size, align)) return false;
		if (elem_size && count > (unsigned long long)LLONG_MAX / elem_size) return false;
		size = (long long)count * elem_size;
		return true;
	}
	if (tk == "strct" || tk == "union") {
		auto it = structIndex_.find(vt.value("type-name", ""));
		if (it == structIndex_.end()) return false;
		const json &entry = capturedStructs_[it->second];
		if (!entry.contains("fields")) return false;
		const json &fields = entry["fields"];
		bool is_union = tk == "union";
		bool packed = entry.value("packed", false);
		long long offset = 0, max_align = entry.value("aligned", 1LL);
		long long tail_bits = 0;  // unused bits of offset's last byte, after a bit-field
		for (size_t i = 0; i < fields.size(); i++) {
			const json &fvt = fields[i]["var-type"];
			bool field_packed = packed || fields[i].value("packed", false);
			long long fsize, falign;
			if (fields[i].contains("bits")) {
				const json &bits = fields[i]["bits"];
				if (bits.is_null() || !typeSizeAlign(fvt, fsize, falign)) return false;
				long long width = bits.get<long long>();
				if (fields[i].contains("name") && !field_packed)
					max_align = max(max_align, falign);
				if (is_union) {
					offset = max(offset, (width + 7) / 8);
				} else {
					long long at = placeCBitField(offset * 8 - tail_bits, width, falign, field_packed);
					long long end = at + width;
					offset = (end + 7) / 8;
					tail_bits = offset * 8 - end;
				}
				continue;
			}
			tail_bits = 0;
			// A flexible array member adds only its alignment.
			if (!is_union && i == fields.size() - 1 && fvt.value("type-kind", "") == "arr"
					&& fvt["size-expr"].is_null()) {
				long long elem_size;
				if (!typeSizeAlign(fvt["base-type"], elem_size, falign)) return false;
				fsize = 0;
			} else if (!typeSizeAlign(fvt, fsize, falign)) {
				return false;
			}
			if (field_packed) falign = 1;
			falign = max(falign, fields[i].value("aligned", 1LL));
			max_align = max(max_align, falign);
			if (is_union) offset = max(offset, fsize);
			else offset = (offset + falign - 1) / falign * falign + fsize;
		}
		size = (offset + max_align - 1) / max_align * max_align;
		align = max_align;
		return true;
	}
	if (tk == "enum") {
		auto it = enumIndex_.find(vt.value("type-name", ""));
		if (it == enumIndex_.end()) return false;
		const CapturedEnum &e = capturedEnums_[it->second];
		if (!e.evaluable) return false;
		size = align = 4;
		for (auto &v : e.values)
			if (v.second < INT32_MIN || v.second > UINT32_MAX) size = align = 8;
		return true;
	}
	return false;
} // LCOV_EXCL_BR_LINE

// Each function in this expression chain (primary_expression .. expression) recognizes
// C expression grammar and, when the (sub)expression is one of a small set of computable
// forms (integer literal, unary +/-, parenthesization, explicit cast, sizeof of a type
// whose layout is known, or a folded binary arithmetic/bitwise operation), also builds a
// value-AST node into `value` ("expr-type": "lit-int" | "cast", mirroring the convention
// documented in doc/ASTSpec.md). Anything else (calls, identifiers, ...) is still recognized
// syntactically (grammar TODOs elsewhere in this chain are unaffected), but `value` is
// left null to signal "not a compile-time constant we can evaluate".
bool CParser::primary_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: string literal, character constant, floating constant
	int index = result_index;

	if (CONSUME(TT_ID)) {
		auto it = enumerators_.find(*tokens[index-1]->info.id);
		if (it != enumerators_.end())
			value = {{"expr-type", "lit-int"}, {"value", to_string(it->second.value)}}; // LCOV_EXCL_BR_LINE
		else
			value = json{};
		result_index = index;
		return true;
	}

	if (CONSUME(TT_PP_NUMBER)) {
		value = parseIntLiteral(*tokens[index-1]->info.str);
		result_index = index;
		return true;
	}

	if (CONSUME_PUNC('(')) {
		if (expression(value, tokens, index)) {
			EXPECT_PUNC(')');
			result_index = index;
			return true;
		}
	}

	return false;
} // LCOV_EXCL_BR_LINE

bool CParser::postfix_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: array subscripting, structure and union member access, postfix increment and decrement
	int index = result_index;

	if (!primary_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC('(')) {
			json arg_value;
			while (assignment_expression(arg_value, tokens, index)) {
				if (!CONSUME_PUNC(',')) {
					break;
				}
			}

			EXPECT_PUNC(')');
			value = json{}; // function call result is not a compile-time constant
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

bool CParser::unary_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: prefix increment and decrement, unary &, unary *, logical NOT
	int index = result_index;

	if (postfix_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	// Unary + and - (e.g. enum initializers like MCHECK_DISABLED = -1)
	bool has_plus = CONSUME_PUNC('+');
	bool has_minus = !has_plus && CONSUME_PUNC('-');
	if (has_plus || has_minus) {
		json inner_value;
		if (!cast_expression(inner_value, tokens, index)) {
			return false;
		}
		if (has_minus) {
			value = foldNegate(inner_value);
		} else {
			value = inner_value; // unary plus: value unchanged
		}
		result_index = index;
		return true;
	}

	if (CONSUME_PUNC('~')) {
		json inner_value;
		if (!cast_expression(inner_value, tokens, index)) {
			return false;
		}
		value = foldBitNot(inner_value);
		result_index = index;
		return true;
	}

	bool is_sizeof = CONSUME_KW(TK_SIZEOF);
	if (is_sizeof || CONSUME_KW(TK_ALIGNOF) || CONSUME_KW(TK_GNU_ALIGNOF)) {
		EXPECT_PUNC('(');

		json slocal;
		if (!declaration_specifiers(slocal, tokens, index)) {
			return false;
		}
		json sdecl = {{"var-type", slocal.value("var-type", json{})}}; // LCOV_EXCL_BR_LINE
		if (!declarator(sdecl, tokens, index, true)) {
			return false;
		}

		EXPECT_PUNC(')');
		long long size, align;
		if (typeSizeAlign(sdecl["var-type"], size, align))
			value = makeTypedLitInt(is_sizeof ? size : align, intPrimType(64, false));
		else
			value = json{};
		result_index = index;
		return true;
	}

	return false;
} // LCOV_EXCL_BR_LINE

bool CParser::cast_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	int save_index = index;
	vector<json> cast_types;

	for (;;) {
		if (!CONSUME_PUNC('(')) {
			break;
		}
		json clocal;
		if (!declaration_specifiers(clocal, tokens, index)) {
			index = save_index; // backtrack
			break;
		}
		json cdecl = {{"var-type", clocal.value("var-type", json{})}}; // LCOV_EXCL_BR_LINE
		if (!declarator(cdecl, tokens, index, true)) {
			index = save_index; // backtrack
			break;
		}

		if (!CONSUME_PUNC(')')) {
			index = save_index; // backtrack
			break;
		}

		cast_types.push_back(cdecl["var-type"]);
		save_index = index;
	}

	// for after cast(s) expression
	json inner_value;
	if (unary_expression(inner_value, tokens, index)) {
		for (auto it = cast_types.rbegin(); it != cast_types.rend(); ++it) {
			int width;
			bool is_signed;
			if (isLitInt(inner_value) && intTypeInfo(*it, width, is_signed))
				inner_value = makeTypedLitInt(litIntBits(inner_value), *it);
			else
				inner_value = {{"expr-type", "cast"}, {"target-type", *it}, {"src", inner_value}}; // LCOV_EXCL_BR_LINE
		}
		value = inner_value;
		result_index = index;
		return true;
	}

	// for not a cast expression
	if (index != result_index) {
		json retry_value;
		if (unary_expression(retry_value, tokens, result_index)) {
			value = retry_value;
			return true;
		}
	}

	return false;
} // LCOV_EXCL_BR_LINE

bool CParser::multiplicative_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!cast_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('*', '/', '%');
		if (!op) break;

		json rhs_value;
		if (!cast_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::additive_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!multiplicative_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('+', '-');
		if (!op) break;

		json rhs_value;
		if (!multiplicative_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::shift_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!additive_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('<<', '>>');
		if (!op) break;

		json rhs_value;
		if (!additive_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::relational_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!shift_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('<', '>', '<=', '>=');
		if (!op) break;

		json rhs_value;
		if (!shift_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::equality_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!relational_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('==', '!=');
		if (!op) break;

		json rhs_value;
		if (!relational_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::and_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!equality_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('&');
		if (!op) break;

		json rhs_value;
		if (!equality_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::exclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!and_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('^');
		if (!op) break;

		json rhs_value;
		if (!and_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::inclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!exclusive_or_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		int op = CONSUME_PUNC_ANY('|');
		if (!op) break;

		json rhs_value;
		if (!exclusive_or_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
	}

	result_index = index;
	return true;
}

bool CParser::logical_and_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical AND operator
	int index = result_index;

	if (inclusive_or_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::logical_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical OR operator
	int index = result_index;

	if (logical_and_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::conditional_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!logical_or_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('?')) {
		json then_value;
		if (!expression(then_value, tokens, index)) {
			return false;
		}
		if (!CONSUME_PUNC(':')) {
			return false;
		}
		json else_value;
		if (!conditional_expression(else_value, tokens, index)) {
			return false;
		}
		value = json{}; // ternary result not evaluated (condition not evaluated)
	}

	result_index = index;
	return true;
}

bool CParser::constant_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (conditional_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::assignment_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement assignment operators
	return conditional_expression(value, tokens, result_index);
}

bool CParser::expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!assignment_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC(',')) {
			// Comma operator: result is the last operand's value (matches C semantics),
			// which falls out naturally since each call below overwrites `value`.
			if (!assignment_expression(value, tokens, index)) {
				return false;
			}
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

// Interprets a value-AST node built by the expression chain above into a (value, type)
// pair, for the narrow set of forms useful as an exported constant: an integer literal,
// or a cast of one (e.g. NULL == ((void *)0)). Anything else (the node is null because
// the source expression wasn't a compile-time constant we track) is rejected.
// `type` stays null when C left the constant untyped, so it reaches Palan as an untyped
// literal that takes its type from the context, like a source literal.
bool CParser::resolveConstValue(const json &node, json &value, json &type)
{
	if (!node.is_object()) return false;

	string expr_type = node.value("expr-type", "");
	if (expr_type == "lit-int") {
		value = node["value"];
		if (node.contains("value-type"))
			type = node["value-type"];
		return true;
	}
	if (expr_type == "cast") {
		json inner_type;
		if (!resolveConstValue(node["src"], value, inner_type)) return false;
		type = node["target-type"];
		return true;
	}

	return false;
}

bool CParser::exportFuncAlias(json &ast, const string &name, const string &target)
{
	json &funcs = ast["ast"]["functions"];
	if (!funcs.is_array()) return false;
	json alias;
	for (const json &f : funcs)
		if (f["name"] == target) { alias = f; break; }
	if (alias.is_null()) return false;

	for (auto it = funcs.begin(); it != funcs.end();)
		it = (*it)["name"] == name ? funcs.erase(it) : it + 1;
	if (!alias.contains("symbol")) alias["symbol"] = target;
	alias["name"] = name;
	funcs.push_back(move(alias));
	return true;
}

void CParser::exportMacroConstants(json &ast, const vector<CMacro*> &macros, CPreprocessor &cpp)
{
	json unexported = json::array();
	for (CMacro* m : macros) {
		if (m->type != MT_OBJ) {
			unexported.push_back({{"name", m->name}, {"function-like", true}}); // LCOV_EXCL_BR_LINE
			continue;
		}
		// "#define X X" over an anonymous enum's X: parse() already exported X.
		auto en = enumerators_.find(m->name);
		if (en != enumerators_.end() && capturedEnums_[en->second.enumIndex].anonymous) continue;

		vector<CToken*> expanded = cpp.expandObjectMacroBody(m);

		json expr_value;
		int index = 0;
		bool ok = constant_expression(expr_value, expanded, index) && index == (int)expanded.size();

		json value, type;
		if (ok) ok = resolveConstValue(expr_value, value, type);
		// A macro that is just a named enum's enumerator stands for that
		// enumerator, so it keeps the enum's type; any expression over one
		// computes as a plain integer.
		if (ok && expanded.size() == 1 && expanded[0]->type == TT_ID) {
			const CapturedEnum &e = capturedEnums_[enumerators_.at(*expanded[0]->info.id).enumIndex];
			if (!e.anonymous)
				type = {{"type-kind", "enum"}, {"type-name", e.name}}; // LCOV_EXCL_BR_LINE
		}

		// "#define stderr stderr": the name means whatever C declares under it.
		bool self_ref = expanded.size() == 1 && expanded[0]->type == TT_ID
		                && *expanded[0]->info.id == m->name;
		string alias_target;
		if (!ok && !self_ref && expanded.size() == 1 && expanded[0]->type == TT_ID)
			alias_target = *expanded[0]->info.id;
		for (CToken* t : expanded) delete t;
		// "#define sscanf __isoc99_sscanf": every C call of the name reaches the
		// target, so the name becomes a declaration with the target's type that
		// links to its symbol, replacing any older declaration of the name.
		if (!alias_target.empty() && exportFuncAlias(ast, m->name, alias_target))
			continue;
		if (!ok) {
			if (!self_ref) unexported.push_back({{"name", m->name}}); // LCOV_EXCL_BR_LINE
			continue;
		}

		json constant = {{"name", m->name}, {"value", value}}; // LCOV_EXCL_BR_LINE
		if (!type.is_null()) constant["value-type"] = type;
		ast["ast"]["constants"].push_back(constant);
	}
	if (!unexported.empty())
		ast["ast"]["unexported-macros"] = move(unexported);
} // LCOV_EXCL_BR_LINE

// Starting point of parsing (top level & included file)
int CParser::parse(json &ast, const vector<CToken*> &tokens)
{
	int index = 0;
	int debug_count = 0;

	if (tokens.size() == 0)
		return 0;
	
	while(index < tokens.size()) {
		if (index >= tokens.size()) return 0;
		furthestTokens = &tokens;
		furthestIndex = index;

		if (declaration(ast, tokens, index, true)) {
			// parsed declaration and function definition
			
		} else {
			const CToken* err_token = tokens[min(furthestIndex, (int)tokens.size() - 1)];
			CLexer* err_lexer = lexers[err_token->lexer_no];
			CToken0& err_t0 = err_lexer->tokens[err_token->token0_no];
			cerr << err_lexer->infile.fname << ":" << err_t0.line_no << ":" << err_t0.pos + 1
				<< ": error: " << PlnC2AstMessage::getMessage(E_UnhandledToken) << endl;
			return 1;
		}
	}
	return 0;
}

// Entry point of parsing
int CParser::parse(json &ast)
{
	auto inherited = [](const map<string, json> &m, const string &name, const json &entry) {
		auto it = m.find(name);
		return it != m.end() && it->second == entry;
	};
	int ret = parse(ast, top_tokens);
	if (ret == 0) {
		json structs = json::array();
		for (const json &s : capturedStructs_)
			if (!inherited(inheritedStructs_, s["name"], s)) structs.push_back(s);
		if (!structs.empty())
			ast["ast"]["structs"] = move(structs);

		json enums = json::array();
		for (const CapturedEnum &e : capturedEnums_) {
			if (!e.evaluable || inherited(inheritedEnums_, e.name, enumState(e))) continue;
			json enumerators = json::array();
			for (auto &[ename, v] : e.values) {
				enumerators.push_back({{"name", ename}, {"value", to_string(v)}}); // LCOV_EXCL_BR_LINE
				if (e.anonymous)
					ast["ast"]["constants"].push_back({{"name", ename}, {"value", to_string(v)}}); // LCOV_EXCL_BR_LINE
			}
			enums.push_back({{"name", e.name}, {"enumerators", move(enumerators)}}); // LCOV_EXCL_BR_LINE
		} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		if (!enums.empty())
			ast["ast"]["enums"] = move(enums);

		json typedefs = json::array();
		for (const auto &[name, vt] : typedefs_) {
			if (inherited(inheritedTypedefs_, name, vt)) continue;
			string tk = vt.value("type-kind", "");
			if (tk == "prim" || tk == "pntr" || tk == "user" || ((tk == "strct" || tk == "union" || tk == "enum") && vt.contains("type-name")))
				typedefs.push_back({{"name", name}, {"var-type", vt}}); // LCOV_EXCL_BR_LINE
		}
		if (!typedefs.empty())
			ast["ast"]["typedefs"] = move(typedefs);
	}
	return ret;
} // LCOV_EXCL_BR_LINE

json CParser::enumState(const CapturedEnum &e) const
{
	json values = json::array();
	for (auto &[ename, v] : e.values) values.push_back({ename, v}); // LCOV_EXCL_BR_LINE
	return {{"name", e.name}, {"anonymous", e.anonymous}, {"evaluable", e.evaluable}, {"values", move(values)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json CParser::dumpState() const
{
	json enums = json::array();
	for (const CapturedEnum &e : capturedEnums_) enums.push_back(enumState(e));
	json enumerators = json::object();
	for (auto &[name, en] : enumerators_) enumerators[name] = {en.value, en.enumIndex}; // LCOV_EXCL_BR_LINE
	return {{"typedefs", typedefs_}, {"structs", capturedStructs_}, {"enums", move(enums)}, // LCOV_EXCL_BR_LINE
	        {"enumerators", move(enumerators)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

void CParser::loadState(const json &state)
{
	for (auto &[name, vt] : state["typedefs"].items()) {
		typedefs_[name] = vt;
		inheritedTypedefs_[name] = vt;
	}
	for (const json &s : state["structs"]) {
		structIndex_[s["name"]] = (int)capturedStructs_.size();
		capturedStructs_.push_back(s);
		inheritedStructs_[s["name"]] = s;
	}
	for (const json &e : state["enums"]) {
		CapturedEnum captured = {e["name"], e["anonymous"], e["evaluable"], {}}; // LCOV_EXCL_BR_LINE
		for (const json &v : e["values"]) captured.values.push_back({v[0], v[1]});
		enumIndex_[captured.name] = (int)capturedEnums_.size();
		capturedEnums_.push_back(move(captured));
		inheritedEnums_[e["name"]] = e;
	}
	for (auto &[name, en] : state["enumerators"].items())
		enumerators_[name] = {en[0], en[1]};
} // LCOV_EXCL_BR_LINE
