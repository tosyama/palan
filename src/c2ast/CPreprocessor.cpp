/// C preprocessor: directives, macro expansion and #include handling.
///
/// @file CPreprocessor.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/assert.hpp>
#include "PlnC2AstMessage.h"

using namespace std;
#include "CFileInfo.h"
#include "CToken.h"
#include "CLexer.h"
#include "CPreprocessor.h"

// utilities
class IfInfo {
public:
	CToken0* start;
	bool is_fulfilled;
	bool is_else;
	IfInfo(CToken0* t): start(t), is_fulfilled(false), is_else(false) {}
};

static int process_include(CPreprocessor& cpp, CLexer& lexer, int n, vector<CToken*>* tokens);
static string headerName(const vector<CToken*>& toks);
static int process_define(CPreprocessor& cpp, CLexer& lexer, int n);
static int process_undef(CPreprocessor& cpp, CLexer& lexer, int n);
static int process_if(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n);
static int process_ifdef(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n, bool is_ndef = false);
static int process_elif(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n);
static int process_else(vector<IfInfo> &ifstack, CLexer& lexer, int n);
static int process_endif(vector<IfInfo> &ifstack, CLexer& lexer, int n);

static int next_pos(const vector<CToken0>& token0s, int n);

//== CMacro ==
CMacro::CMacro(CMacroType type, string name, int lexer_no)
	: type(type), name(name), variadic(false), lexer_no(lexer_no), inherited(false)
{
}

//== CPreprocessor ==
void CPreprocessor::loadMacros(CFileInfo finfo)
{
	// Registered like a preprocessed file: macro body tokens point back into
	// this lexer, and CParser looks tokens' locations up through `lexers`.
	int lexer_no = lexers.size();
	lexers.push_back(new CLexer(finfo)); // LCOV_EXCL_BR_LINE
	CLexer& lexer = *lexers.back();
	lexer.no = lexer_no;

	vector<CToken0>& token0s = lexer.scan();
	for (int n=0; n<token0s.size(); n++) {
		CToken0 *t = &token0s[n];
		if (t->type == TT0_MACRO) {		// '#'
			n = next_pos(token0s, n);
			BOOST_ASSERT(t != &token0s[n]);
			t = &token0s[n];
			BOOST_ASSERT(t->type == TT0_ID);
			string directive = lexer.get_str(t);
			if (directive == "define") {
				n = process_define(*this, lexer, n);
			} else if (directive == "undef") {
				n = process_undef(*this, lexer, n);
			} else {
				BOOST_ASSERT(directive == "pragma");
				n = next_pos(token0s, n);
				BOOST_ASSERT(lexer.get_str(&token0s[n]) == "palan_once_included");
				n = next_pos(token0s, n);
				BOOST_ASSERT(token0s[n].type == TT0_STR);
				string path = lexer.get_str(&token0s[n]);
				once_included.insert(path.substr(1, path.size() - 2));
			}
		}
	}
	for (CMacro* m: macros) m->inherited = true;
}

bool CPreprocessor::preprocess(const string& filepath, vector<CToken*> *tokens)
{
	if (!CFileInfo::fileExists(filepath)) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_IncludeNotFound, filepath));
	}

	if (!tokens) {
		tokens = &top_tokens;
	}
	int lexer_no = lexers.size();
	CFileInfo finfo(filepath);
	lexers.push_back(new CLexer(finfo)); // LCOV_EXCL_BR_LINE
	CLexer& lexer = *lexers.back();
	lexer.no = lexer_no;

	vector<IfInfo> ifstack;

	vector<CToken0>& token0s = lexer.scan();

	bool debug = false;

	for (int n=0; n<token0s.size(); n++) {
		BOOST_ASSERT(!macro_stack.size());
		CToken0 *t0 = &token0s[n];
		if (t0->type == TT0_MACRO) {	// '#'
			if (t0->is_eol) continue;	// null directive

			n = next_pos(token0s, n);
			BOOST_ASSERT(t0 != &token0s[n]);
			t0 = &token0s[n];

			if (t0->type == TT0_ID) {
				string directive = lexer.get_str(t0);
				if (directive == "include") {
					n = process_include(*this, lexer, n, tokens); 
					
				} else if (directive == "include_next") {
					cerr << lexer.infile.fname << ":" << t0->line_no
						<< ": warning: '#include_next' is not supported, ignored" << endl;
					while (!token0s[n].is_eol) n = next_pos(token0s, n);

				} else if (directive == "define") {
					n = process_define(*this, lexer, n);

				} else if (directive == "undef") {
					n = process_undef(*this, lexer, n);

				} else if (directive == "if") {
					n = process_if(ifstack, *this, lexer, n);

				} else if (directive == "ifdef") {
					n = process_ifdef(ifstack, *this, lexer, n);

				} else if (directive == "ifndef") {
					n = process_ifdef(ifstack, *this, lexer, n, true);

				} else if (directive == "elif") {
					n = process_elif(ifstack, *this, lexer, n);

				} else if (directive == "else") {
					n = process_else(ifstack, lexer, n);

				} else if (directive == "endif") {
					n = process_endif(ifstack, lexer, n);

				} else if (directive == "line") {
					if (line_warned.count(lexer.infile.fname) == 0) {
						line_warned.insert(lexer.infile.fname);
						cerr << lexer.infile.fname << ":" << t0->line_no
							<< ": warning: '#line' is not supported, ignored" << endl;
					}
					while (!token0s[n].is_eol) n = next_pos(token0s, n);

				} else if (directive == "pragma") {
					n = next_pos(token0s, n);
					CToken0 &prag_t = token0s[n];
					if (prag_t.type == TT0_ID && lexer.get_str(&prag_t) == "once") {
						once_included.insert(lexer.infile.fname);
					}
					while (!token0s[n].is_eol) n = next_pos(token0s, n);
				} else if (directive == "warning") {
					string msg = "";
					while (!token0s[n].is_eol) {
						n = next_pos(token0s, n);
						msg += lexer.get_str(&token0s[n]) + " ";
					}
					cerr << lexer.infile.fname << ":" << t0->line_no
						<< ": warning: " << msg << endl;
				} else if (directive == "error") {
					string msg = "";
					while (!token0s[n].is_eol) {
						n = next_pos(token0s, n);
						msg += lexer.get_str(&token0s[n]) + " ";
					}
					throw CPreprocessError(PlnC2AstMessage::getMessage(E_ErrorDirective, msg),
						lexer.no, (int)(t0 - &token0s[0]));
				} else {
					cerr << lexer.infile.fname << ":" << t0->line_no
						<< ": warning: unsupported directive '#" << directive << "', ignored" << endl;
					while (!token0s[n].is_eol) n = next_pos(token0s, n);
				}

			} else {
				throw CPreprocessError(PlnC2AstMessage::getMessage(E_InvalidDirective),
					lexer.no, (int)(t0 - &token0s[0]));
			}

		} else {
			list<CToken* > unprocessed_tokens;

			// collect tokens until next macro directive
			do {
				BOOST_ASSERT(t0->type != TT0_MACRO);
				CToken* t = lexer.createToken(n);
				if (t)	// skip comment
					unprocessed_tokens.push_back(t);

				n++;

				if (n >= token0s.size())
					break;
				t0 = &token0s[n];
			} while (t0->type != TT0_MACRO);

			n--; // for n++ in for loop

			// process unprocessed tokens
			vector<CToken*> processed_tokens = scan_macro(unprocessed_tokens);
			tokens->insert(tokens->end(), processed_tokens.begin(), processed_tokens.end() );
		}
	}
	return true;
}

vector<list<CToken*> > extract_macro_func_args(list<CToken*> &tokens, const CMacro* m)
{
	// Extract macro function arguments from tokens starting at front (the position of '(')
	// Remove argument tokens from tokens and return them as vector<list<CToken*>>

	vector<list<CToken*> > args;
	BOOST_ASSERT(tokens.front()->type == TT_PUNCTUATOR && tokens.front()->info.punc == '(');
	delete tokens.front();
	tokens.pop_front(); // remove '('
	int blace_level = 1;
	list<CToken*> arg;

	while (!tokens.empty()) {
		CToken* t = tokens.front();
		tokens.pop_front();

		if (t->type == TT_PUNCTUATOR) {
			if (t->info.punc == '(') {
				blace_level++;
			} else if (t->info.punc == ')') {
				blace_level--;
				if (blace_level == 0) {
					// end of arguments
					delete t;
					args.push_back(arg);
					break;
				}
			}

			bool in_va_arg = m->variadic && args.size() == m->params.size() - 1;
			if (blace_level == 1 && t->info.punc == ',' && !in_va_arg) {
				// end of current argument
				delete t;
				args.push_back(arg);
				arg.clear();
				continue;
			}
		}
		arg.push_back(t);
	}

	if (m->variadic && args.size() < m->params.size())
		args.resize(m->params.size());	// variadic part omitted: F(a) for F(a, ...)

	return args;
}

static bool token_spelling(const CToken* t, string& s);

CToken* get_defined_id_token(list<CToken*> &unprocessed_tokens)
{
	// expect '(' ID ')' or ID
	if (unprocessed_tokens.empty()) return NULL;

	// Keywords are identifiers to the preprocessor, e.g. "defined(__attribute__)".
	auto is_identifier = [](CToken* t) { return t->type == TT_ID || t->type == TT_KEYWORD; };
	CToken* t = unprocessed_tokens.front();
	if (is_identifier(t)) {
		unprocessed_tokens.pop_front();
		return t;
	}

	// expect '('
	if (t->type != TT_PUNCTUATOR) return NULL;
	if (t->info.punc != '(') return NULL;
	unprocessed_tokens.pop_front(); // remove '('
	delete t;

	// expect ID
	if (unprocessed_tokens.empty()) return NULL;
	CToken* id_token = unprocessed_tokens.front();
	if (!is_identifier(id_token)) return NULL;
	unprocessed_tokens.pop_front(); // remove ID

	// expect ')'
	// TODO: handle error(delete id_token)
	if (unprocessed_tokens.empty()) return NULL;
	t = unprocessed_tokens.front();
	if (t->type != TT_PUNCTUATOR) return NULL;
	if (t->info.punc != ')') return NULL;
	unprocessed_tokens.pop_front(); // remove ')'
	delete t;

	return id_token;
}

// `__has_include ( header )` after its name `t`, as the number 1 or 0.
CToken* CPreprocessor::hasInclude(const CToken* t, list<CToken*> &unprocessed_tokens)
{
	auto isPunc = [](const CToken* p, int c) { return p->type == TT_PUNCTUATOR && p->info.punc == c; };
	vector<CToken*> operand;
	bool closed = false;
	if (!unprocessed_tokens.empty() && isPunc(unprocessed_tokens.front(), '(')) {
		delete unprocessed_tokens.front();
		unprocessed_tokens.pop_front();
		while (!unprocessed_tokens.empty()) {
			CToken* p = unprocessed_tokens.front();
			unprocessed_tokens.pop_front();
			if (isPunc(p, ')')) { delete p; closed = true; break; }
			operand.push_back(p);
		}
	}
	string name = closed ? headerName(operand) : "";
	for (CToken* p : operand) delete p;
	if (name.empty())
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_InvalidHasIncludeSyntax),
			t->lexer_no, t->token0_no);
	bool found = CFileInfo::getFilePath(name, lexers[t->lexer_no]->infile.fname, include_paths) != "";
	CToken* number = new CToken(TT_PP_NUMBER, t->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
	number->info.str = new string(found ? "1" : "0"); // LCOV_EXCL_BR_LINE
	return number;
} // LCOV_EXCL_BR_LINE

vector<CToken*> CPreprocessor::scan_macro(list<CToken*> &unprocessed_tokens, bool in_if_directive)
{
	vector<CToken*> result_tokens;
	while (!unprocessed_tokens.empty()) {
		CToken* t = unprocessed_tokens.front();

		// Ahead of macro expansion, which would replace `linux` in <linux/a.h>.
		// predefined.h still defines __has_include, for "#ifdef __has_include".
		if (in_if_directive && t->type == TT_ID && *t->info.id == "__has_include") {
			unprocessed_tokens.pop_front();
			result_tokens.push_back(hasInclude(t, unprocessed_tokens));
			delete t;
			continue;
		}
		if (t->type == TT_ID) {
			CMacro* m = macro_exists(*t->info.id);
			// A function-like macro's name not followed by '(' stays a plain name.
			auto next = std::next(unprocessed_tokens.begin());
			bool invoked = m && (m->type != MT_FUNC || (next != unprocessed_tokens.end()
				&& (*next)->type == TT_PUNCTUATOR && (*next)->info.punc == '('));
			if (invoked && !t->does_hide(m)) {
				unprocessed_tokens.pop_front(); // remove macro name token
				vector<list<CToken*> > args;
				if (m->type == MT_FUNC) {
					args = extract_macro_func_args(unprocessed_tokens, m);
				}
				vector<CToken*> expanded_tokens = expand_macro_tokens(m, args, t->hide_set);

				//result_tokens.insert(result_tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				unprocessed_tokens.insert(unprocessed_tokens.begin(), expanded_tokens.begin(), expanded_tokens.end());
				delete t;
				continue; // re-scan from the top
			}
			if (in_if_directive && *t->info.id == "defined") {
				unprocessed_tokens.pop_front(); // remove "defined" token

				// expect '(' ID ')' or ID
				CToken* id_token = get_defined_id_token(unprocessed_tokens);
				if (!id_token) {
					int err_lexer_no = t->lexer_no;
					int err_token0_no = t->token0_no;
					delete t;
					throw CPreprocessError(PlnC2AstMessage::getMessage(E_InvalidDefinedSyntax),
						err_lexer_no, err_token0_no);
				}

				string id_name;
				token_spelling(id_token, id_name);
				CMacro* dm = macro_exists(id_name);
				if (dm) {
					CToken* one_token = new CToken(TT_PP_NUMBER, id_token->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
					one_token->info.str = new string("1"); // LCOV_EXCL_BR_LINE
					result_tokens.push_back(one_token);
				} else {
					CToken* zero_token = new CToken(TT_PP_NUMBER, id_token->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
					zero_token->info.str = new string("0"); // LCOV_EXCL_BR_LINE
					result_tokens.push_back(zero_token);
				}

				delete t;
				delete id_token;
				continue; // re-scan from the top
			}
		}

		unprocessed_tokens.pop_front();
		result_tokens.push_back(t);
	}
	return result_tokens;
}

// Expands an object-like macro's own body in isolation (no surrounding token stream),
// e.g. to resolve `#define S_IFDIR __S_IFDIR` down to a numeric literal for constant
// export. Copies are seeded into `m`'s own hide set before scanning so self/circular
// references (`#define X X`, or mutual `A`/`B`) stop cleanly instead of looping.
vector<CToken*> CPreprocessor::expandObjectMacroBody(CMacro *m)
{
	list<CToken*> unprocessed;
	for (CToken* t : m->body) {
		CToken* copy = new CToken(*t); // LCOV_EXCL_BR_LINE
		copy->add_to_hide_set(m);
		unprocessed.push_back(copy);
	}
	return scan_macro(unprocessed);
}

// '#' and a digit can't spell one C identifier, so a placeholder never names a
// macro and survives every rescan.
int CPreprocessor::placeholderIndex(const CToken *t)
{
	if (t->type != TT_ID) return -1;
	const string &s = *t->info.id;
	if (s.size() < 2 || s[0] != '#') return -1;
	int n = 0;
	for (size_t i = 1; i < s.size(); i++) {
		if (!isdigit((unsigned char)s[i])) return -1;
		n = n * 10 + (s[i] - '0');
	}
	return n;
}

vector<CToken*> CPreprocessor::expandFunctionMacroBody(CMacro *m, string &unsupported)
{
	if (m->variadic) { unsupported = "__VA_ARGS__"; return {}; }
	for (CToken* t : m->body) {
		if (t->type != TT_PUNCTUATOR) continue;
		if (t->info.punc == '#') { unsupported = "#"; return {}; }
		if (t->info.punc == '##') { unsupported = "##"; return {}; }
	}
	vector<list<CToken*> > args;
	for (size_t i = 0; i < m->params.size(); i++) {
		CToken* p = new CToken(TT_ID, m->lexer_no, 0); // LCOV_EXCL_BR_LINE
		p->info.id = new string("#" + to_string(i)); // LCOV_EXCL_BR_LINE
		args.push_back({p});
	}
	vector<CToken*> body = expand_macro_tokens(m, args, NULL);
	list<CToken*> unprocessed(body.begin(), body.end());
	return scan_macro(unprocessed);
}

vector<CToken*> replace_parameter_tokens(CToken* t, const vector<string>& params, const vector<list<CToken*> >& args)
{
	// replace parameter tokens in t and return as vector<CToken*>
	// if t is not parameter token, return vector with t itself
	vector<CToken*> result;
	if (t->type == TT_ID) {
		for (int p=0; p < params.size(); p++) {
			if (*t->info.id == params[p]) {
				// replace parameter by argument tokens
				// copy args[p] tokens
				for (CToken* at: args[p]) {
					result.push_back(new CToken(*at)); // LCOV_EXCL_BR_LINE
				}

				if (result.empty()) {
					// if argument is empty, insert empty token
					CToken* empty_token = new CToken(TT_ID, t->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
					empty_token->info.id = new string(""); // LCOV_EXCL_BR_LINE
					result.push_back(empty_token);
				}
				return result;
			}
		}
	}
	// not parameter token
	result.push_back(t);
	return result;
}

static string punc_to_str(int punc)
{
	if (punc > 0xFFFF)
		return string({(char)(punc>>16), (char)(punc>>8), (char)punc});
	if (punc > 0xFF)
		return string({(char)(punc>>8), (char)punc});
	return string(1, (char)punc);
}

// false for a token kind that has no source spelling (TT_INCLUDE etc.).
static bool token_spelling(const CToken* t, string& s)
{
	switch (t->type) {
		case TT_ID:         s = *t->info.id; return true;
		case TT_KEYWORD:    s = CLexer::keywords[t->info.keyword]; return true;
		case TT_PP_NUMBER:
		case TT_STR:
		case TT_CHAR:       s = *t->info.str; return true;
		case TT_PUNCTUATOR: s = punc_to_str(t->info.punc); return true;
		default:            return false;
	}
}

static string token_paste_str(CToken* t, const vector<CLexer*>& lexers)
{
	string s;
	if (t->type == TT_STR || t->type == TT_CHAR || !token_spelling(t, s))
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroPasteInvalidToken),
			t->lexer_no, t->token0_no);
	return s;
}

static string stringify_arg(const list<CToken*>& arg, const vector<CLexer*>& lexers)
{
	string result = "\"";
	bool first = true;
	for (CToken* t : arg) {
		if (!first) result += " ";
		first = false;
		if (t->type == TT_STR || t->type == TT_CHAR) {
			for (char c : *t->info.str) {
				if (c == '\\' || c == '"') result += '\\';
				result += c;
			}
		} else {
			string s;
			if (!token_spelling(t, s))
				throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroStringifyInvalidToken),
					t->lexer_no, t->token0_no);
			result += s;
		}
	}
	result += "\"";
	return result;
}

static void process_stringify(vector<CToken*>& body_tokens, const vector<string>& params, const vector<list<CToken*>>& args, const vector<CLexer*>& lexers)
{
	for (int n = 0; n < (int)body_tokens.size(); n++) {
		CToken* t = body_tokens[n];
		if (t->type != TT_PUNCTUATOR || t->info.punc != '#') continue;

		BOOST_ASSERT(n + 1 < (int)body_tokens.size()); // '#' at end not allowed
		CToken* next = body_tokens[n + 1];
		BOOST_ASSERT(next->type == TT_ID); // '#' must be followed by param name

		int param_idx = -1;
		for (int p = 0; p < (int)params.size(); p++) {
			if (*next->info.id == params[p]) { param_idx = p; break; }
		}
		BOOST_ASSERT(param_idx >= 0); // param not found

		string str_val = stringify_arg(args[param_idx], lexers);
		CToken* str_token = new CToken(TT_STR, t->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
		str_token->info.str = new string(str_val); // LCOV_EXCL_BR_LINE

		delete t;
		delete next;
		body_tokens.erase(body_tokens.begin() + n, body_tokens.begin() + n + 2);
		body_tokens.insert(body_tokens.begin() + n, str_token);
	}
}

bool process_paste(vector<CToken*>& body_tokens, const vector<string>& params, const vector<list<CToken*> >& args, const vector<CLexer*>& lexers)
{
	if (body_tokens.size() >= 3) {
		if (body_tokens[0]->type == TT_PUNCTUATOR && body_tokens[0]->info.punc == '##') {
			throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroPasteAtStart),
				body_tokens[0]->lexer_no, body_tokens[0]->token0_no);
		}

		for (int n=1; n < (int)body_tokens.size(); n++) {
			CToken *t = body_tokens[n];
			if (t->type == TT_PUNCTUATOR && t->info.punc == '##') {
				if (n == (int)body_tokens.size() - 1) {
					throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroPasteAtEnd),
						t->lexer_no, t->token0_no);
				}
				CToken *t_prev = body_tokens[n-1];
				CToken *t_next = body_tokens[n+1];

				vector<CToken*> prev_tokens = replace_parameter_tokens(t_prev, params, args);
				vector<CToken*> next_tokens = replace_parameter_tokens(t_next, params, args);
				t_prev = prev_tokens.back();
				prev_tokens.pop_back();
				t_next = next_tokens.front();
				next_tokens.erase(next_tokens.begin());

				string pasted_str = token_paste_str(t_prev, lexers) + token_paste_str(t_next, lexers);
				CToken* pasted_token;
				if (isdigit(pasted_str[0]) || (pasted_str[0] == '.' && pasted_str.size() > 1 && isdigit(pasted_str[1]))) {
					pasted_token = new CToken(TT_PP_NUMBER, t->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
					pasted_token->info.str = new string(move(pasted_str));
				} else {
					pasted_token = new CToken(TT_ID, t->lexer_no, t->token0_no); // LCOV_EXCL_BR_LINE
					pasted_token->info.id = new string(pasted_str); // LCOV_EXCL_BR_LINE
					for (int i = 0; i < TK_NOT_KEYWORD; i++) {
						if (pasted_str == CLexer::keywords[i]) {
							pasted_token->type = TT_KEYWORD;
							delete pasted_token->info.id;
							pasted_token->info.keyword = (CTokenKeyword)i;
							break;
						}
					}
				}
				
				body_tokens.erase(body_tokens.begin() + n - 1, body_tokens.begin() + n + 2);
				body_tokens.insert(body_tokens.begin() + n - 1, prev_tokens.begin(), prev_tokens.end());
				body_tokens.insert(body_tokens.begin() + n - 1 + prev_tokens.size(), pasted_token);
				body_tokens.insert(body_tokens.begin() + n - 1 + prev_tokens.size() + 1, next_tokens.begin(), next_tokens.end());
				return true;
			}
		}
	}

	return false;
}

vector<CToken*> CPreprocessor::expand_macro_tokens(CMacro *m, vector<list<CToken*> > &args, vector<CMacro*> *hide_set)
{
	vector<CToken*> body_tokens;
	// copy macro body
	for (int n=0; n < m->body.size(); n++) {
		CToken *t = new CToken(*m->body[n]); // LCOV_EXCL_BR_LINE
		t->add_to_hide_set(m); // add to hide set
		if (hide_set) {
			for (CMacro* hm: *hide_set) {
				t->add_to_hide_set(hm);
			}
		}
		body_tokens.push_back(t);
	}

	// process stringizing '#'
	process_stringify(body_tokens, m->params, args, lexers);

	// process token pasting '##'
	while(process_paste(body_tokens, m->params, args, lexers))
		;

	// expand arguments
	vector<vector<CToken*> > args_expanded;
	for (list<CToken*> &arg: args) {
		vector<CToken*> arg_tokens = scan_macro(arg);
		args_expanded.push_back(arg_tokens);
	}

	// replace parameters in body_tokens
	for (int n=0; n<body_tokens.size(); n++) {
		CToken *t = body_tokens[n];
		if (t->type == TT_ID) {
			for (int p=0; p < m->params.size(); p++) {
				if (*t->info.id == m->params[p]) {
					// replace parameter by argument tokens
					// copy args[p] tokens
					vector<CToken*> arg_copied;
					for (CToken* at: args_expanded[p]) {
						arg_copied.push_back(new CToken(*at)); // LCOV_EXCL_BR_LINE
					}
					delete t;
					body_tokens.erase(body_tokens.begin() + n);
					body_tokens.insert(body_tokens.begin() + n, arg_copied.begin(), arg_copied.end());
					n += arg_copied.size() - 1;
					break;
				}
			}
		}
	}

	for (vector<CToken*> &arg: args_expanded) {
		for (CToken* at: arg) {
			delete at;
		}
	}

	return body_tokens;
}

//== utilities ==
void is_not_eol(CToken0& t)
{
	if (t.is_eol) {
		BOOST_ASSERT(false);
	}
}

// The header named by tokens `"a.h"` or `<a.h>`, as getFilePath takes it; empty
// for anything else.
static string headerName(const vector<CToken*>& toks)
{
	if (toks.size() == 1 && toks[0]->type == TT_STR) return *toks[0]->info.str;
	if (toks.empty() || toks.front()->type != TT_PUNCTUATOR || toks.front()->info.punc != '<'
			|| toks.back()->type != TT_PUNCTUATOR || toks.back()->info.punc != '>')
		return "";
	string name;
	for (CToken* t : toks) {
		switch (t->type) {
			case TT_ID:      name += *t->info.id; break;
			case TT_PUNCTUATOR: name += (char)t->info.punc; break;
			case TT_PP_NUMBER:  name += *t->info.str; break;
			default: break;
		}
	}
	return name;
}

int process_include(CPreprocessor& cpp, CLexer& lexer, int n, vector<CToken*>* tokens)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	n = next_pos(token0s, n);
	CToken0 &fname_t = token0s[n];

	string inc_path;
	if (fname_t.type == TT0_STR) {
		inc_path = lexer.get_str(&fname_t);
	} else {
		// Macro expansion: collect tokens until EOL then expand
		list<CToken*> unprocessed;
		int m = n;
		while (true) {
			CToken* t = lexer.createToken(m);
			if (t) unprocessed.push_back(t);
			if (token0s[m].is_eol) break;
			m = next_pos(token0s, m);
		}
		n = m;

		vector<CToken*> expanded = cpp.scan_macro(unprocessed);
		inc_path = headerName(expanded);
		if (inc_path.empty()) {
			for (CToken* t : expanded) delete t;
			throw CPreprocessError(PlnC2AstMessage::getMessage(E_IncludeExpectsFilename),
				lexer.no, (int)(&fname_t - &token0s[0]));
		}
		for (CToken* t : expanded) delete t;

		string foundpath = CFileInfo::getFilePath(inc_path, lexer.infile.fname, cpp.include_paths);
		if (foundpath == "") {
			throw CPreprocessError(PlnC2AstMessage::getMessage(E_IncludeNotFound, inc_path),
				lexer.no, (int)(&fname_t - &token0s[0]));
		}
		if (cpp.once_included.count(foundpath) == 0) {
			cpp.preprocess(foundpath, tokens);
		}
		return n;
	}

	string foundpath = CFileInfo::getFilePath(inc_path, lexer.infile.fname, cpp.include_paths);

	if (foundpath == "") {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_IncludeNotFound, inc_path),
			lexer.no, (int)(&fname_t - &token0s[0]));
	}

	if (cpp.once_included.count(foundpath) == 0) {
		cpp.preprocess(foundpath, tokens);
	}

	if (!fname_t.is_eol) {
		n = next_pos(token0s, n);
		if (!token0s[n].type == TT0_COMMENT || !token0s[n].is_eol) {
			throw CPreprocessError("extra tokens at end of '#include' directive",
				lexer.no, n);
		}
	}

	return n;
}

static void delete_macro(vector<CMacro*> &macros, string &macro_name);
static int parse_params(CMacro& m, CLexer& lexer, int n);
static int set_macro_body(vector<CToken*> &body, CLexer& lexer, int n);

int process_define(CPreprocessor& cpp, CLexer& lexer, int n)
{
	CFileInfo &curfile = lexer.infile;
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	n = next_pos(token0s, n);
	CToken0 &t = token0s[n];

	if (t.type != TT0_ID) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroNameNotIdentifier),
			lexer.no, n);
	}
	string macro_name = lexer.get_str(&t);
	delete_macro(cpp.macros, macro_name);

	if (t.is_eol) {	// define only name
		cpp.macros.push_back(new CMacro(MT_OBJ, lexer.get_str(&t), lexer.no)); // LCOV_EXCL_BR_LINE
		return n;
	}

	n = next_pos(token0s, n);
	CToken0 &t1 = token0s[n];

	if (t1.type == TT0_COMMENT) {	// define only name
		BOOST_ASSERT(t1.is_eol);
		cpp.macros.push_back(new CMacro(MT_OBJ, macro_name, lexer.no)); // LCOV_EXCL_BR_LINE
		return n;

	} else if (t1.type == TT0_MACRO_ARGS) {	// define function like macro
		CMacro* func_macro = new CMacro(MT_FUNC, macro_name, lexer.no); // LCOV_EXCL_BR_LINE
		n = parse_params(*func_macro, lexer, n);
		n = set_macro_body(func_macro->body, lexer, n);
		cpp.macros.push_back(func_macro);

		return n;

	} else {
		CMacro* macro = new CMacro(MT_OBJ, macro_name, lexer.no); // LCOV_EXCL_BR_LINE
		CToken* t = lexer.createToken(n);
		macro->body.push_back(t);
		n = set_macro_body(macro->body, lexer, n);
		cpp.macros.push_back(macro);

		return n;
	}
}

void delete_macro(vector<CMacro*> &macros, string &macroname)
{
	auto macro_i = find_if(macros.begin(), macros.end(), [&](CMacro* m) { return m->name == macroname; } );
	if (macro_i != macros.end()) {
		delete *macro_i;
		macros.erase(macro_i);
	}
}

int parse_params(CMacro& m, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);	// NG: #define hoge(

	auto is_ellipsis = [&](CToken0& t) {
		return t.type == TT0_PUNCTUATOR && lexer.get_ch(&t) == '...';
	};
	auto malformed = [&](int n) {
		return CPreprocessError(PlnC2AstMessage::getMessage(E_MalformedMacroArgList),
			lexer.no, n);
	};

	for (;;) {
		n = next_pos(token0s, n);
		CToken0 &t = token0s[n];

		if (t.type == TT0_ID || is_ellipsis(t)) {
			is_not_eol(t);
			if (t.type == TT0_ID) {
				m.params.push_back(lexer.get_str(&t));
			} else {
				m.params.push_back("__VA_ARGS__");
				m.variadic = true;
			}
			n = next_pos(token0s, n);

			if (!m.variadic && is_ellipsis(token0s[n])) {	// GNU named variadic: NAME...
				is_not_eol(token0s[n]);
				m.variadic = true;
				n = next_pos(token0s, n);
			}

			CToken0 &t_next = token0s[n];
			if (t_next.type != TT0_PUNCTUATOR)
				throw malformed(n);
			int c = lexer.get_ch(&t_next);
			if (c == ')')
				break;
			if (c != ',' || m.variadic)
				throw malformed(n);

		} else if (t.type == TT0_PUNCTUATOR && lexer.get_ch(&t) == ')') {
			break;
		} else {
			throw malformed(n);
		}
	}

	return n;
}

int set_macro_body(vector<CToken*> &body, CLexer& lexer, int n)
{
	CToken0 *t0 = &lexer.tokens[n];
	if (t0->is_eol)
		return n;

	do {
		n++;
		CToken* t = lexer.createToken(n);
		if (t) {
			body.push_back(t);
		}
		t0 = &lexer.tokens[n];
	} while (!t0->is_eol);

	return n;
}

int process_undef(CPreprocessor& cpp, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;

	is_not_eol(token0s[n]);

	n = next_pos(token0s, n);
	CToken0 &t = token0s[n];

	if (t.type != TT0_ID) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroNameNotIdentifier),
			lexer.no, n);
	}

	string macro_name = lexer.get_str(&t);
	delete_macro(cpp.macros, macro_name);

	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			cerr << lexer.infile.fname << ":" << token0s[n].line_no << ":" << token0s[n].pos + 1
				<< ": " << PlnC2AstMessage::getMessage(W_ExtraTokensAfterDirective, "undef") << endl;
		}
	}

	return n;
}

static int skip_to_nextif(vector<IfInfo>& ifstack, CLexer& lexer, int n);
static bool evaluate_condition(vector<CToken*> &tokens, const vector<CLexer*> &lexers);

int process_if(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;

	ifstack.emplace_back(&token0s[n]);
	list<CToken*> unprocessed_tokens;
	vector<CToken*> condition_tokens;

	while (!token0s[n].is_eol) {
		n++;
		CToken *t = lexer.createToken(n);
		if (t) {
			unprocessed_tokens.push_back(t);
		}
	}

	condition_tokens = cpp.scan_macro(unprocessed_tokens, true);

	ifstack.back().is_fulfilled = evaluate_condition(condition_tokens, cpp.lexers);

	if (!ifstack.back().is_fulfilled) {
		n = skip_to_nextif(ifstack, lexer, n);
	}

	return n;
}

CMacro* CPreprocessor::macro_exists(const string& id)
{
	auto mi = find_if(macros.begin(), macros.end(),
			[id](CMacro* m) { return m->name == id; });

	if (mi != macros.end()) {
		return *mi;
	}
	return NULL;
}

// Condition parser definition
// expr     = lor ("?" expr ":" expr)?
// lor      = land ("||" land)*
// land     = bor ("&&"  bor)*
// bor      = bxor ("|" bxor)*
// bxor     = band ("^" band)*
// band     = equality ("&" equality)*
// equality   = relational ("==" relational | "!=" relational)*
// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
// shift      = add ("<<" add | ">>" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary | "%" unary)*
// unary      = ("+" | "-" | "~" | "!")? primary
// primary    = num | "(" expr ")" | id(=0)

static long expr(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long lor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long land(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long bor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long bxor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long band(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long equality(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long relational(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long shift(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long add(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long mul(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long unary(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static long primary(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers);
static bool consume(vector<CToken*> &tokens, int &n, int ch);

#define END_CHECK	{ \
	if (n >= (int)tokens.size()) { \
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_IfExprUnexpectedEnd)); \
	} \
}
#define CONSUME(ch)	consume(tokens, n, (ch))

bool evaluate_condition(vector<CToken*> &tokens, const vector<CLexer*> &lexers)
{
	int n = 0;
	int x = expr(tokens, n, lexers);
	if (n < (int)tokens.size()) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_IfExprExtraTokens),
			tokens[n]->lexer_no, tokens[n]->token0_no);
	}
	return x != 0;
}

long expr(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	int x = lor(tokens, n, lexers);
	if (CONSUME('?')) {
		int y = expr(tokens, n, lexers);
		if (!CONSUME(':')) {
			int pos = n < (int)tokens.size() ? n : n-1;
			throw CPreprocessError(PlnC2AstMessage::getMessage(E_IfExprMissingColon),
				tokens[pos]->lexer_no, tokens[pos]->token0_no);
		}
		int z = expr(tokens, n, lexers);
		return x ? y : z;
	}
	return x;
}

long lor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = land(tokens, n, lexers);
	for (;;) {
		if (CONSUME('||')) {
			long rhs = land(tokens, n, lexers);
			x = x || rhs;
		} else {
			break;
		}
	}
	return x;
}

long land(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = bor(tokens, n, lexers);
	for (;;) {
		if (CONSUME('&&')) {
			long rhs = bor(tokens, n, lexers);
			x = x && rhs;
		} else {
			break;
		}
	}
	return x;
}

long bor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = bxor(tokens, n, lexers);
	for (;;) {
		if (CONSUME('|')) {
			x = x | bxor(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long bxor(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = band(tokens, n, lexers);
	for (;;) {
		if (CONSUME('^')) {
			x = x ^ band(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long band(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = equality(tokens, n, lexers);
	for (;;) {
		if (CONSUME('&')) {
			x = x & equality(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long equality(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = relational(tokens, n, lexers);
	for (;;) {
		if (CONSUME('==')) {
			x = x == relational(tokens, n, lexers);
		} else if (CONSUME('!=')) {
			x = x != relational(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long relational(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = shift(tokens, n, lexers);
	for (;;) {
		if (CONSUME('<')) {
			x = x < shift(tokens, n, lexers);
		} else if (CONSUME('<=')) {
			x = x <= shift(tokens, n, lexers);
		} else if (CONSUME('>')) {
			x = x > shift(tokens, n, lexers);
		} else if (CONSUME('>=')) {
			x = x >= shift(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long shift(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = add(tokens, n, lexers);
	for (;;) {
		if (CONSUME('<<')) {
			x = x << add(tokens, n, lexers);
		} else if (CONSUME('>>')) {
			x = x >> add(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long add(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = mul(tokens, n, lexers);
	for (;;) {
		if (CONSUME('+')) {
			x = x + mul(tokens, n, lexers);
		} else if (CONSUME('-')) {
			x = x - mul(tokens, n, lexers);
		} else {
			break;
		}
	}

	return x;
}

long mul(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;
	int x = unary(tokens, n, lexers);
	for (;;) {
		if (CONSUME('*')) {
			x = x * unary(tokens, n, lexers);
		} else if (CONSUME('/')) {
			x = x / unary(tokens, n, lexers);
		} else if (CONSUME('%')) {
			x = x % unary(tokens, n, lexers);
		} else {
			break;
		}
	}
	return x;
}

long unary(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;

	if (CONSUME('+')) {
		return primary(tokens, n, lexers);
	} else if (CONSUME('-')) {
		return -primary(tokens, n, lexers);
	} else if (CONSUME('~')) {
		return ~primary(tokens, n, lexers);
	} else if (CONSUME('!')) {
		return !primary(tokens, n, lexers);
	}

	return primary(tokens, n, lexers);
}

long primary(vector<CToken*> &tokens, int &n, const vector<CLexer*> &lexers)
{
	END_CHECK;

	long x = 0;

	if (CONSUME('(')) {
		x = expr(tokens, n, lexers);
		if (!CONSUME(')')) {
			int pos = n > 0 ? n - 1 : 0;
			throw CPreprocessError(PlnC2AstMessage::getMessage(E_IfExprMissingParen),
				tokens[pos]->lexer_no, tokens[pos]->token0_no);
		}
		return x;
	}

	CToken *t = tokens[n];
	CTokenType tt = t->type;

	if (tt == TT_PP_NUMBER) {
		// NOTE: stoll throws std::out_of_range for values that exceed INT64_MAX
		// (e.g. 0xffffffffffffffffUL). Correct fix requires __int128 arithmetic
		// throughout the evaluator, which is not yet implemented.
		x = stoll(*t->info.str, nullptr, 0);
	} else if (tt == TT_CHAR) {
		// character-constant: evaluate inner value (e.g. 'a' -> 97)
		const string& s = *t->info.str;
		if (s.size() >= 3 && s[1] == '\\') {
			switch (s[2]) {
				case 'n': x = '\n'; break; case 't': x = '\t'; break;
				case 'r': x = '\r'; break; case '0': x = '\0'; break;
				case '\\': x = '\\'; break; case '\'': x = '\''; break;
				default: x = s[2]; break;
			}
		} else if (s.size() >= 3) {
			x = s[1];
		}
	} else if (tt == TT_ID) {
		x = 0;
	} else {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_IfExprInvalidToken),
			t->lexer_no, t->token0_no);
	}

	n++;
	return x;
}

bool consume(vector<CToken*> &tokens, int &n, int ch)
{
	if (n >= tokens.size()) {
		return false;
	}
	CToken *t = tokens[n];
	if (t->type == TT_PUNCTUATOR && t->info.punc == ch) {
		n++;
		return true;
	}
	return false;
}

int process_ifdef(vector<IfInfo>& ifstack, CPreprocessor& cpp, CLexer& lexer, int n, bool is_ndef)
{
	vector<CToken0> &token0s = lexer.tokens;

	is_not_eol(token0s[n]);

	ifstack.emplace_back(&token0s[n]);
	n = next_pos(token0s, n);
	
	CToken0 &t = token0s[n];
	if (t.type != TT0_ID) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_MacroNameNotIdentifier),
			lexer.no, n);
	}

	string macro_name = lexer.get_str(&t);
	auto& macros = cpp.macros;
	auto macro_i = find_if(macros.begin(), macros.end(), [&](CMacro* m) { return m->name == macro_name; } );
	if (is_ndef) {
		ifstack.back().is_fulfilled = macro_i == macros.end();
	} else {
		ifstack.back().is_fulfilled = macro_i != macros.end();
	}

	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			cerr << lexer.infile.fname << ":" << token0s[n].line_no << ":" << token0s[n].pos + 1
				<< ": " << PlnC2AstMessage::getMessage(W_ExtraTokensAfterDirective, "ifdef") << endl;
		}
	}

	if (!ifstack.back().is_fulfilled) {
		n = skip_to_nextif(ifstack, lexer, n);
	}

	return n;
}

int process_elif(vector<IfInfo> &ifstack, CPreprocessor &cpp, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_ElifWithoutIf),
			lexer.no, n);
	}

	if (ifstack.back().is_fulfilled) {
		return skip_to_nextif(ifstack, lexer, n);
	}

	if (ifstack.back().is_else) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_ElifAfterElse),
			lexer.no, n);
	}

	ifstack.pop_back();
	return process_if(ifstack, cpp, lexer, n);
}

int process_else(vector<IfInfo> &ifstack, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_ElseWithoutIf),
			lexer.no, n);
	}

	vector<CToken0> &token0s = lexer.tokens;
	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			cerr << lexer.infile.fname << ":" << token0s[n].line_no << ":" << token0s[n].pos + 1
				<< ": " << PlnC2AstMessage::getMessage(W_ExtraTokensAfterDirective, "else") << endl;
		}
	}

	if (ifstack.back().is_fulfilled) {
		ifstack.back().is_else = true;
		n = skip_to_nextif(ifstack, lexer, n);
	} else {
		ifstack.back().is_fulfilled = true;
		ifstack.back().is_else = true;
	}

	return n;
}

int process_endif(vector<IfInfo> &ifstack, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		throw CPreprocessError(PlnC2AstMessage::getMessage(E_EndifWithoutIf),
			lexer.no, n);
	}

	ifstack.pop_back();

	vector<CToken0> &token0s = lexer.tokens;
	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			cerr << lexer.infile.fname << ":" << token0s[n].line_no << ":" << token0s[n].pos + 1
				<< ": " << PlnC2AstMessage::getMessage(W_ExtraTokensAfterDirective, "endif") << endl;
		}
	}

	return n;
}

int skip_to_nextif(vector<IfInfo>& ifstack, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	n++;

	while (n < token0s.size()) {
		CToken0 *t = &token0s[n];
		while (t->type != TT0_MACRO) {
			n++;
			if (n >= token0s.size())
				return n;
			t = &token0s[n];
		}

		BOOST_ASSERT(t->type == TT0_MACRO);

		if (t->is_eol) {
			n++;
			continue;
		}

		int m = next_pos(token0s, n);

		t = &token0s[m];
		if (t->type != TT0_ID) {
			n = m;
			continue;
		}

		string directive = lexer.get_str(t);
		if (directive == "if"
				|| directive == "ifdef"
				|| directive == "ifndef") {
			ifstack.emplace_back(t);
			ifstack.back().is_fulfilled = true;
			n = skip_to_nextif(ifstack, lexer, n);

		} else if (directive == "elif"
				|| directive == "else") {
			if (ifstack.back().is_else) {
				throw CPreprocessError(PlnC2AstMessage::getMessage(E_ElseAfterElse),
					lexer.no, m);
			}
			if (ifstack.back().is_fulfilled) {
				n = m+1;
				continue;	// skip
			}
			return n-1;	// back to before '#'

		} else if (directive == "endif") {
			return process_endif(ifstack, lexer, m);
		}
		n++;
	}

	return n;
}

int next_pos(const vector<CToken0>& token0s, int n)
{
	BOOST_ASSERT(!token0s[n].is_eol);

	// Search next token (Skip comment and end search at eol.)
	const CToken0 *t = NULL;
	do {
		n++;
		t = &token0s[n];
	} while(t->type == TT0_COMMENT && !t->is_eol);

	return n;
}

void CPreprocessor::outputError(const CPreprocessError& err)
{
	if (err.lexer_no >= 0 && err.token0_start >= 0) {
		CLexer &lexer = *lexers[err.lexer_no];
		CToken0 &t = lexer.tokens[err.token0_start];
		string& line = lexer.infile.lines[t.line_no-1];
		cerr << lexer.infile.fname << ":"
			<< t.line_no << ":" << t.pos+1 << ": error: "
			<< err.what() << endl;
		cerr << line << endl;

		for (int i=0; i<t.pos; i++) {
			cerr << ' ';
		}
		cerr << '^' << endl;

	} else {
		cerr << "error: " << err.what() << endl;
	}
}

CPreprocessError::CPreprocessError(const string& what, int lexer_no, int start, int end)
	: runtime_error(what), lexer_no(lexer_no), token0_start(start), token0_end(end)
{
}

void CPreprocessor::dumpMacroState(ostream& os)
{
	for (CMacro* m: macros) {
		os << "#define " << m->name;
		if (m->type == MT_FUNC) {
			os << "(";	// no space: a space would make it object-like
			for (int i=0; i<m->params.size(); i++) {
				os << (i ? ", " : "");
				bool is_va = m->variadic && i == m->params.size() - 1;
				if (!is_va || m->params[i] != "__VA_ARGS__")
					os << m->params[i];
				if (is_va)
					os << "...";
			}
			os << ")";
		}
		for (CToken* t: m->body) {
			string s;
			bool has_spelling = token_spelling(t, s);
			BOOST_ASSERT(has_spelling);
			os << " " << s;
		}
		os << "\n";
	}
	for (const string& path: once_included)
		os << "#pragma palan_once_included \"" << path << "\"\n";
}

void CPreprocessor::dumpPreprocessed(ostream& os)
{
	for (CToken* t: top_tokens) {
		if (t->type == TT_ID) {
			os << *t->info.id;
		} else if (t->type == TT_PP_NUMBER || t->type == TT_CHAR) {
			os << *t->info.str;
		} else if (t->type == TT_PUNCTUATOR) {
			os << (char)(t->info.punc);
		} else if (t->type == TT_STR) {
			os << *t->info.str;
		} else if (t->type == TT_KEYWORD) {
			BOOST_ASSERT(t->info.keyword < TK_NOT_KEYWORD);
			os << CLexer::keywords[t->info.keyword] << " ";
		} else {
			os << ".";
		}
	}

}


