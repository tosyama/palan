/// C preprocessor: directives, macro expansion and #include handling.
///
/// @file CPreprocessor.h
/// @copyright 2026 YAMAGUCHI Toshinobu

class CPreprocessError : public runtime_error {
public:
	int lexer_no;
	int token0_start;
	int token0_end;
	CPreprocessError(const string& what, int lexer_no=-1, int start=-1, int end=-1); 
};

typedef enum {
	MT_OBJ,
	MT_FUNC,
} CMacroType;

class CMacro {
public:
	CMacroType type;
	string name;
	vector<string> params;
	bool variadic;	// the last of params takes the remaining arguments; unnamed `...` is __VA_ARGS__
	vector<CToken*> body;
	int lexer_no;
	bool inherited;	// loaded by loadMacroFile, not defined by the header

	CMacro(CMacroType type, string name, int lexer_no);
};

class CPreprocessor {
public:
	vector<CToken*> top_tokens;
	vector<CLexer*> lexers;
	vector<CMacro*> &macros;
	vector<CMacro*> macro_stack;
	vector<string> &include_paths;
	ostream *out_stream;
	set<string> once_included;
	set<string> line_warned;

	CPreprocessor(vector<CMacro*> &macros, vector<string> &include_paths)
		: macros(macros), include_paths(include_paths), out_stream(&cout) {}
	~CPreprocessor() {
		for (CLexer* l: lexers) delete l;
	}

	CMacro* macro_exists(const string& id);
	CToken* hasInclude(const CToken* t, list<CToken*> &unprocessed_tokens);
	vector<CToken*> scan_macro(list<CToken*> &unprocessed_tokens, bool in_if_directive=false);
	vector<CToken*> expand_macro_tokens(CMacro *m, vector<list<CToken*>> &args, vector<CMacro*> *hide_set);
	vector<CToken*> expandObjectMacroBody(CMacro *m);

	// Loads #define/#undef lines and the "#pragma palan_once_included" lines
	// dumpMacroState writes; used for predefined.h and for a c-state's macros.
	// Every macro defined once it returns is marked inherited.
	void loadMacros(CFileInfo finfo);
	void dumpMacroState(ostream& os);
	// true: success, false: failed
	bool preprocess(const string& filepath, vector<CToken*> *tokens=NULL);
	void outputError(const CPreprocessError& err);

	void dumpPreprocessed(ostream& os);
};
