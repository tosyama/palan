/// C token definitions.
///
/// @file CToken.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <cstdint>

typedef enum {
	TT0_ID,
	TT0_PUNCTUATOR,
	TT0_STR,
	TT0_NUMBER,
	TT0_CHAR,
	TT0_COMMENT,
	TT0_MACRO,
	TT0_MACRO_ARGS,
} CToken0Type;

class CToken0 {
public:
	CToken0Type type;
	int32_t line_no;
	int16_t pos;
	int16_t len;
	bool is_eol;

	CToken0(CToken0Type type, int line_no, int pos, int len);
};

typedef enum {
	TT_ID,
	TT_KEYWORD,
	TT_PUNCTUATOR,
	TT_STR,

	TT_PP_NUMBER,
	TT_CHAR,

	TT_NOT_VALID_TOKEN,
} CTokenType;

typedef enum {
	TK_TYPEDEF,
	TK_EXTERN,
	TK_STATIC,

	TK_CONST,

	TK_SIZEOF,
	TK_ALIGNOF,
	TK_GNU_ALIGNOF,

	TK_SIGNED,
	TK_UNSIGNED,
	TK_SHORT,
	TK_LONG,
	TK_INT,
	TK_CHAR,
	TK_FLOAT,
	TK_DOUBLE,
	TK_STRUCT,
	TK_UNION,
	TK_ENUM,
	TK_VOID,
	TK_BOOL,
	TK_COMPLEX,
	TK_INT128,

	TK_INLINE,
	TK_RESTRICT,
	TK_VOLATILE,

	TK_ATTRIBUTE,

	TK_NOT_KEYWORD,
} CTokenKeyword;

class CMacro; // forward declaration

class CToken {
public:
	CTokenType type;
	int16_t lexer_no;
	int token0_no;
	union {
		string *id;
		CTokenKeyword keyword;
		int punc;
		int ch;
		string *str;
	} info;
	vector<CMacro*> *hide_set;

	CToken(CTokenType type, int lexer_no, int token0_no);
	CToken(const CToken& token);
	CToken(const string& s, int lexer_no, int token0_no);
	~CToken();

	void add_to_hide_set(CMacro* m);
	bool does_hide(CMacro* m);
};
