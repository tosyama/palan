/// Palan Semantic Analyzer
///
/// @file PlnSemanticAnalyzer.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <map>
#include <deque>
#include <memory>
#include <set>
#include <vector>
#include <optional>
#include <functional>
#include <filesystem>
#include <cstdint>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"
#include "PlnSaMessage.h"

using namespace std;

using json = nlohmann::json;

struct FieldLayout {
	string name;
	string typeKind;   // "prim" | "embed" | "owned" | "raw-ptr" | "embed-arr" | "embed-varr"
	string typeName;   // prim type name, embed struct name
	int    offset;     // byte offset from struct start (C ABI aligned)
	int    size;       // prim: type size, embed: sub-struct totalSize, owned/raw-ptr: 8,
	                    // embed-arr: the array's bytes
	string  enumName = "";  // enum a primitive is typed as, if any
	// A C type this version cannot represent (e.g. "function pointer"): the
	// field keeps its place in the layout but cannot be accessed.
	string  unsupported = "";
	// An owned field (a struct `T` or an array `[n]...`) is a slot holding the
	// block it owns, typed as a variable holding that block is; an embed-arr
	// field (`$[n]...`) is the array laid out in the struct, typed as a
	// variable whose array is laid out in a frame. A raw-ptr field is typed as
	// a variable of its pointer or borrow type is.
	json    type;
	json    initAst;   // the field's initializer as a literal of typed constants; null when it has none
	// "private" or "readonly" toward code outside the module defining the
	// struct; empty = public.
	string  access;
	// A C bit-field: `width` bits from bit `shift` of the unit of its type at
	// `offset`; width 0 is not a bit-field.
	int     bitShift = 0;
	int     bitWidth = 0;
};

// Where an aggregate variable's outermost storage block lives: `$T v;` puts it
// in the function's stack frame.
enum class VarStorage { Heap, Frame };
// A copy makes a new value whole, or writes into one that exists, which a
// type's '->' does for the parts of its type (SpecAndDesign.md 3.8).
enum class CopyInto { New, Existing };

// The allocator helpers build-mgr generates (SpecAndDesign.md 3.6), one
// family per shape: __pln_<op>_<struct>, and for an array of any element E,
// __pln_<op>_arr_<E>, __pln_<op>_varr_<E> and __pln_<op>_evarr_<E> (`[+]$T`),
// E written as the symbol of its layout.
enum class AllocFamily { Struct, Arr, Varr, EVarr, Dict, EDict };
// elem is the struct for Struct, and otherwise the element as the array's
// storage lays it out (layoutElem).
struct AllocShapeRef { AllocFamily family; json elem; };

// A declared array's storage type, and the lengths of its own levels as SA
// expressions, outermost first; a length known only at run time is absent from
// the type.
struct Storage { json type; vector<json> lens; };
// How an aggregate variable's own block is obtained and released on the heap,
// and what it takes in a stack frame (see placeAggregateVar).
struct AggregatePlace {
	json heapInit;
	json release;
	int64_t frameBytes;
	int frameAlign;
	optional<AllocShapeRef> shape;
	json build;   // what builds structs with initializers laid out in the block, once obtained
};

struct StructDef {
	string name;
	vector<FieldLayout> fields;
	int  totalSize = -1;
	int  maxAlign  = 0;
	bool ownsFields = false;  // has an owned field
	// Has a field initializer, its own or one of a struct laid out in it, so it
	// is built wherever it comes to exist.
	bool hasInits = false;
	bool built() const { return ownsFields || hasInits; }
	// The symbol of its own '->', which it carries to every module it reaches.
	string copyFunc;
	// An opaque type: owned through one pointer like a struct, laid out as its
	// contents, which are null where they are not known (a `.pai`). It is
	// released through releaseFunc, which its module defines.
	bool   isOpaque = false;
	json   contents;       // as a declaration's resolved var-type
	json   contentsOwned;  // as its owner holds it
	string releaseFunc;
	// false = tag is known but its layout isn't (C incomplete-type equivalent, e.g. FILE).
	// Only usable through a pointer (@T/@!T); buildStructDef sets this true on success.
	bool   isComplete = false;
	string incompleteReason;  // "unsupported-field" or "forward-declared" when !isComplete; empty otherwise
	// A C union: every field sits at offset 0. Only layout (buildStructDef) and
	// diagnostics consult this; every other consumer sees offsets/totalSize.
	bool   isUnion = false;
	// A field (or one nested in an embedded struct) sits off its type's own
	// alignment, as only a C packed struct allows; passed by value as MEMORY class.
	bool   hasUnalignedField = false;
	// A Palan union: a uint32 tag at offset 0 (an unnamed field) and each
	// variant's contents, overlaid as a C union's fields are, so that what reads
	// the layout sees every byte any variant may hold. What builds, releases or
	// copies one acts on the variant the tag names. `variants` lists every
	// variant in tag order, those without contents too.
	bool   isTagged = false;
	vector<string> variants;
	const char* keyword() const { return isUnion || isTagged ? "union" : "struct"; }
};

struct FieldChain {
	bool   isPointerBased;  // false = var+offset (inline chain), true = ptr-expr needed
	string varName;         // base variable name when !isPointerBased
	int    offset;          // accumulated byte offset
	json   ptrExpr;         // partial SA json when isPointerBased
	string structName;      // struct type currently being resolved
};

// A function a call can name: from a cinclude (isC) or a Palan definition.
struct Callee {
	const json* def;
	bool        isC;
};

// A call argument before the callee's parameter is known. A pending one gets
// its type only from the parameter (see takesExpectedType), so it is analyzed
// once the callee is chosen.
// A chained value (`v -> f()`) becomes `v` or `@v` by the parameter, so it is
// pending too. probe is what a pending argument is matched by: a literal's
// default type, or a chained value's analysis.
struct CallArg {
	json ast;
	bool pending;
	json sa;
	bool chained = false;
	json probe;
	bool spread = false;  // `...a`: ast is a
};

// The outcome of choosing among overloads: the function chosen, or else the
// ones tied for it (none when no function fits).
struct CalleeChoice {
	optional<Callee> chosen;
	vector<Callee>   tied;
};

// Narrowed: exact but for a writable outermost pointer given as read-only.
// Converted: fits only once a literal is converted to the parameter's struct.
enum class ArgFit { None, Converted, Viable, Narrowed, Exact };

// A diagnostic decided without exiting, so a check can also just test a fit.
struct SaDiag {
	PlnSaMessageCode code;
	string arg1 = "\x01";
	string arg2 = "\x01";
};

// The value of a constant expression under evaluation.
struct ConstVal {
	enum class Kind { Int, Uint, Flo } kind;
	int64_t  i = 0;
	uint64_t u = 0;
	long double f = 0;  // the widest float, flo80, so a flo80 constant keeps its precision
	bool hex = false;  // an Int that is a bit pattern, as a hexadecimal literal is
};

class PlnSemanticAnalyzer {
	string c2astPath;
	string inputFilePath;
	string moduleId_;  // the module's identity, a type's origin
	bool checks_ = true;  // a debug build's run-time checks
	json sa;
	PlnTypeRegistry registry_;
	map<string, string> strLiteralLabels;  // value -> label
	const json*       currentFunc_    = nullptr;  // null = _start level
	const json*       currentFuncDef_ = nullptr;  // currentFunc_'s types resolved, not normalized
	string            retSlot_;                   // the variable a single returned struct is written into
	// Prefix of the symbols of functions defined here: the module, or the
	// enclosing function's symbol, plus `..b<n>` inside a block.
	string            symbolScope_;
	int               blockSymbolSeq_ = 0;       // blocks with functions seen in this function
	int               loopDepth_     = 0;        // nesting depth of while loops
	size_t            funcBodyScopeIdx_ = 0;     // 0 = top-level (no function)

	vector<map<string, json>> varScopes;
	vector<map<string, json>> cFuncScopes;
	vector<map<string, json>> cGlobalScopes;
	// Expandable definitions by name; a null entry ends an outer one's.
	vector<map<string, json>> expandableScopes;
	// A deque keeps every definition where it is while overloads are added.
	vector<map<string, deque<json>>> plnFuncScopes;
	// The builtins of each `[+]T` and `[""]T` element type, outside every
	// scope: they exist for every T, and are registered as each T is first used.
	map<string, deque<json>> builtins_;
	set<string> builtinTypes_;
	// scope stack: alias("" = unqualified) → funcname → funcDefs (several
	// when unqualified imports of different modules export the same name)
	vector<map<string, map<string, vector<json>>>> importScopes;

	// Aggregate variables per scope (parallel to varScopes) and what runs when
	// each goes out of scope. A frame variable is listed even with nothing to
	// release, but only a heap variable owns its storage.
	// A global's release never runs: what it owns lives as long as the process.
	struct ScopeVar { string name; json release; bool owned; bool global = false; };
	vector<vector<ScopeVar>> arrayScopeVars_;
	// Stack of while-body scope indices (for break/continue cleanup)
	vector<size_t> whileScopeStack_;
	// What each enclosing loop runs before its next iteration; a 'continue' runs it too.
	vector<json> loopSteps_;
	set<string> forIndexVars_;
	// A top-level `global` variable of this module.
	struct GlobalVar {
		string            label;
		json              type;
		const json*       decl;
		json              view[2];  // an array or struct as a function sees it: [writable]; else null
		map<string, bool> funcs;    // permitted top-level function name -> writable
	};
	map<string, GlobalVar> globals_;
	// Set once an alias import brings a global, so `M.count` may appear.
	bool qualifiedGlobals_ = false;
	// The C globals a `global` statement has named, with the functions it permits.
	map<string, map<string, bool>> cGlobalPermits_;
	// This module's top-level Palan functions: written name -> their symbols.
	map<string, vector<string>> topFuncSymbols_;
	// The top-level function being analyzed, by written name; empty elsewhere.
	string permitFunc_;
	// The names the function being analyzed binds to static storage at its
	// entry ({name, label, var-type}), and the Palan globals among them it may
	// only read.
	json staticBindings_ = json::array();
	map<string, bool> readOnlyGlobals_;  // -> true for an array or struct, never replaced
	// The module's `..init`, whose scope holds the globals declared so far:
	// their initializers run there, before any top-level statement.
	vector<map<string, json>> initVarScopes_ = {{}};
	vector<vector<ScopeVar>>  initScopeVars_ = {{}};
	json                      initBindings_ = json::array();
	json                      initBody_ = json::array();
	vector<string>            initImports_;  // module identities, in import order
	// While a global's initializer is analyzed: the top-level scope it cannot
	// use, and the global's name.
	const map<string, json>*  globalInitTopScope_ = nullptr;
	string                    globalInitName_;
	// The location each enclosing 'for' iterates over, as a path from its
	// variable: "." + field, or "[]" for any index. Its storage and length are
	// held for the whole loop.
	// A union whose contents an `is` binds is pinned too, with the variant
	// bound: its variant must not change. `shown` names a hidden location.
	struct PinnedRange { vector<string> path; bool resizable; bool borrow; string variant = ""; string shown = ""; };
	vector<PinnedRange> pinnedRanges_;
	// Transfer is into the location, Give out of it.
	enum class RangeChange { Borrow, Copy, Transfer, Give };
	// The pointer an `is` binding reads a union through, and the variant the
	// test found, so reading it needs no check.
	map<string, string> knownVariants_;
	// What the `is` bindings of the condition being analyzed need: hidden
	// variables declared ahead of the statement, in a scope of their own, and
	// the names bound, declared for the body.
	struct IsBound { string name; json type; json loc; };
	struct IsScope { json prefix = json::array(); vector<IsBound> bound; };
	IsScope* isScope_ = nullptr;
	// Counter for generating unique temporary variable names
	int tempVarCounter_ = 0;
	// The top level names the implicit `args`; its header is built only then.
	bool argsUsed_ = false;
	// Set while a call argument giving its value away (`x>>`) is analyzed; a
	// `>>` anywhere else is an error.
	bool transferAllowed_ = false;
	// Variables of the function being analyzed (the top level counts as one)
	// whose storage a pointer has been given to, and the count of
	// `pending-alias` statements waiting for that to be known.
	set<string> escapedVars_;
	int pendingAliases_ = 0;
	// Which local variables have given their whole value away (`->>`, `>>`,
	// `<<=`), along the statements' flow. `since` is the loop entry count when
	// the variable was last declared or filled; a use in a loop entered after
	// it reaches that loop's previous iteration. Names are unique among the
	// scopes in effect (declareVar), so a name identifies its variable.
	struct GivenState { bool given = false; int since = 0; };
	struct GivenFlow { map<string, GivenState> vars; bool reachable = true; };
	struct GivenLoop {
		int entry;
		map<string, json> exposed;  // first use reaching the loop's head, by name
		GivenFlow entry_flow, back{{}, false}, exit{{}, false};
	};
	struct GivenTracking {
		GivenFlow flow;
		vector<GivenLoop> loops;
		// Applied when the statement ends: the same statement may be analyzed
		// twice (pending-alias), and the arguments after `x>>` may borrow x.
		vector<pair<string, bool>> pending;  // name, given (else filled)
		int seq = 0;
	};
	GivenTracking given_;
	bool nullCompare_ = false;  // the variable being analyzed is compared with NULL
	// Registered struct type definitions
	map<string, StructDef> structDefs_;
	// C struct tag -> the typedef name messages call it by (from c2ast).
	map<string, string>    cStructDisplayNames_;
	set<string>            elemShapeKeys_;    // the same for element alloc-shapes
	set<string>            allocShapeNames_;  // dedup guard for struct alloc-shapes
	// __pln_B<n> -> n: the struct an element struct owning nothing is laid out as.
	map<string, int>       byteTypes_;
	// Written type name -> the type it names in this scope, in program-wide
	// names: aliases, C typedefs, enums, and every Palan struct source may
	// write here. An unbound C struct is written by its program-wide name.
	map<string, json>      typeAliases_;
	// cinclude'd C typedefs (name -> var-type, its own references resolved), for
	// resolving a later header's "user" reference to one of them.
	map<string, json>      cTypedefs_;
	// Registered enum types: enumerator values, and the declaration's loc,
	// null for a cinclude'd C enum (whose first definition wins).
	struct EnumDef { map<string, int64_t> values; json loc; };
	map<string, EnumDef>   enumDefs_;
	// Defining module of each Palan struct and enum identity. Build-mgr keys
	// struct allocators by these names, so one comes from one module only. A C
	// type or an alias has no entry.
	map<string, string>    typeOrigins_;
	// Imported type names (as written in their module) source may not write
	// unqualified -> the module alias to write them with.
	map<string, string>    hiddenTypeNames_;
	// A top-level struct/enum/alias/const declaration, resolved when first
	// needed, so neither source order nor which of two files importing each
	// other is analyzed first decides whether a type is known yet.
	struct TopDecl {
		const json* stmt;
		enum State { Pending, Resolving, Done } state = Pending;
		int    pos = -1;      // index among the module's top-level statements; -1 in a block
		string deferredBy;    // a const left to its statement: the name its value waits for
	};
	map<string, TopDecl>   topDecls_;
	// The top-level struct declarations a '->' of this module is written for.
	// Whether a type has a '->' decides how it is laid out and copied, which a
	// type declaration may ask before any function is registered.
	set<const json*>       copyFuncStructs_;
	// Opaque types of this module -> the symbol of their `release` function.
	map<string, string>    releaseFuncs_;
	// A top-level variable, whose type a const's value may need before the
	// statements are analyzed.
	struct TopVar {
		const json* stmt;
		size_t      idx;      // in the statement's vars
		int         pos;
		TopDecl::State state = TopDecl::Pending;
		json        type;     // null when its declaration's shape does not fix it
	};
	map<string, TopVar>    topVars_;
	// A type a top-level import makes writable here, keyed by how it is
	// written (`T` or `V.T`).
	struct ImportedType {
		const json*          importStmt;
		PlnSemanticAnalyzer* module;
		const json*          decl;  // the exporting module's declaration
		bool                 adopted = false;
	};
	map<string, ImportedType> importedTypes_;
	// A declaration a module publishes: its own, or one an `export import`
	// passes on, with the module that declares it.
	struct ModuleExport {
		enum Kind { Type, Func, Global };
		Kind                 kind;
		PlnSemanticAnalyzer* module;
		const json*          decl;
		string name() const;
	};
	optional<vector<ModuleExport>> exports_;
	// Every importer reads what this module publishes through this one list.
	const vector<ModuleExport>& exports();
	void collectExports(vector<ModuleExport>& out, set<const json*>& seen, set<string>& visiting);
	// Struct names declared at the top level here or imported, resolved or
	// not: a pointer may name one before its layout is known.
	set<string>            declaredStructs_;
	// Top-level declarations the pre-scan resolved; step 2 skips them.
	set<const json*>       prescannedDecls_;
	// A block's declarations join topDecls_, typeAliases_ and constDecls_
	// under their written names while the block is analyzed. Shadowing is an
	// error, so leaving the block erases them without hiding an outer entry.
	vector<vector<string>> blockDeclNames_;
	// Written name -> identity of each top-level struct/enum and each one of
	// the enclosing blocks. Modules and blocks may reuse a name, the allocators
	// and sa.json may not.
	map<string, string>    typeIdentities_;
	int                    blockTypeSeq_ = 0;
	// Type names (`T` or `V.T`) each enclosing block's imports made writable.
	vector<vector<string>> blockImportedTypes_;
	// Program-wide type resolution state, shared by every module's analyzer.
	struct TypeProgram;
	unique_ptr<TypeProgram> ownedProgram_;
	TypeProgram*            program_;
	json                    moduleAst_;  // an imported module's AST, which topDecls_ point into
	const json*             moduleAstRoot_ = nullptr;  // this module's AST, wherever it is held
	// Registered const declarations (name -> {"value": <SA'd literal expr>, "value-type": <type>})
	map<string, json>      constDecls_;
	// Library names collected from cinclude `link` clauses. A set: the same
	// library named by two cinclude statements is emitted once, and the
	// ordering makes the sa.json "libs" section deterministic.
	set<string>            linkLibs_;
	set<string>            linkObjs_;  // absolute paths, resolved by gen-ast

	void enterScope();
	void leaveScope();
	json collectFreeStmts(size_t from_idx, size_t to_idx, const string& keep = "");

	string locPrefix(const json& node) const;
	void   rejectArrowOperand(const json& expr, const string& op) const;

	void        declareVar(const string& name, const json& type, const json* loc_node = nullptr);
	const json* findVar(const string& name) const;
	// findVar, or else a C global, which the current function then binds.
	// `use` is false only for the variable `->>` fills.
	const json* lookupVar(const string& name, const json& locNode, bool use = true);
	void        checkNotGiven(const string& name, const json& locNode);
	void        checkRootNotGiven(const json& location);
	void        applyPendingGiven();
	static void mergeGivenFlow(GivenFlow& into, const GivenFlow& from);
	void        enterGivenLoop();
	void        leaveGivenLoop();
	void        bindStatic(const string& name, const string& label, const json& type);
	void        requireAssignable(const string& name, const json& locNode) const;
	bool        isStaticBound(const string& name) const;
	json        storageBorrowType(const string& name, const json& varType, bool isMutable) const;
	void        sa_global_decl(const json& stmt);
	// The type an exported global has wherever it is imported, from its declaration
	// alone: the initializer gives only an array literal's sizes.
	json        exportedGlobalType(const json& stmt);
	// Declares a global in the current scope and binds it to its label.
	void        bindGlobal(const string& name, const string& label, const json& type,
	                       const optional<ScopeVar>& scopeVar, const json* locNode);
	// Lets `global` statements name a global bound at the top level.
	void        registerGlobal(const string& name, const string& label, const json& type, const json* decl);
	void        importGlobals(const json& stmt, PlnSemanticAnalyzer& sub);
	// `M.count` of an imported global, written as a field access, becomes its
	// id, so later passes meet one shape whatever reads the statement raw.
	json        resolveQualifiedGlobals(const json& node) const;
	void        sa_global_permit(const json& stmt);
	void        declarePermittedGlobals(const string& funcName, const json& funcDef);
	void        emitInitFunc();
	bool        isLocalVar(const string& name) const;
	bool        isOwnedVar(const string& name) const;
	bool        isFrameVar(const string& name) const;
	void        removeFromArrayScope(const string& name);
	void        addOwnedScopeVar(const string& name, json release);
	void        noteImplicitArgs(const json& stmts);
	json        implicitArgsDecl() const;

	void        registerCFunc(const string& name, const json& def);
	const json* findCFunc(const string& name) const;

	void        registerCGlobal(const string& name, const json& def);
	const json* findCGlobal(const string& name) const;
	const json* lookupCGlobal(const string& name, const json& locNode);

	string      normalizeExpandableBody(json& node) const;
	void        registerExpandables(const json& stmt);
	const json* findExpandable(const string& name) const;
	vector<Callee> findCCallees(const string& name) const;
	string      unreachableCName(const json& body) const;
	json        expandCall(const json& expr, const json& def, const json& args, SaDiag& why);
	json        expandedCall(const json& expr, const string& alias, const string& name);
	[[noreturn]] void undefinedNameError(const json& at, const json& ref, PlnSaMessageCode undefinedCode) const;

	void        registerPlnFunc(const string& name, const json& def, const json& locNode);
	const json* findPlnFuncDef(const json& funcDef) const;
	string calleeKey(const json& def, bool isC) const;
	vector<Callee> findCallees(const string& name) const;
	vector<Callee> findCalleesByAlias(const string& alias, const string& name) const;
	size_t countCallees(const json& call) const;
	void   requireAlias(const json& locNode, const string& alias) const;
	CalleeChoice chooseCallee(const string& name, const vector<Callee>& cands, const vector<CallArg>& args);
	Callee selectCallee(const json& locNode, const string& name, const vector<Callee>& cands,
	                    const vector<CallArg>& args);
	ArgFit argFit(const CallArg& arg, const json& param, size_t argIdx, bool isCFunc, const string& funcName);
	bool structLitFits(const json& lit, const json& paramVT);
	ArgFit typeFit(const json& argVT, const json& paramVT);
	bool   sameShape(const CallArg& arg, const json& paramVT, bool isCFunc);
	string argTypesText(const vector<CallArg>& args);
	string defaultText(const json& d) const;
	string calleesText(const string& name, const vector<Callee>& cands) const;
	json   makeCall(const json& expr, const vector<Callee>& cands, const string& writtenName);
	vector<Callee> findCalleesUnderAnyAlias(const string& name) const;
	vector<Callee> forProtocolCallees(const string& name, const json& structVT) const;
	vector<Callee> subscriptCallees(const string& op, const json& structVT) const;
	json   convertibleLiteral(const json& ast, bool toUnion);
	bool   isUnionPntr(const json& vt) const;
	vector<Callee> literalConversions(const json& structVT, const json& lit);
	json   literalConversion(const json& ast, const json& toVT);
	vector<Callee> usableCallees(const json& expr, const vector<Callee>& cands) const;

	json sa_statements(const json& stmts);
	void beginModule(const json& ast);
	void prescanTypes(const json& stmts, const json& funcs);
	void indexTopDecls(const json& stmts);
	void indexCopyFuncs(const json& funcs);
	json loadImportAst(const json& stmt) const;
	// The imported module's type context, shared program-wide; its types are
	// resolved only as they are needed.
	PlnSemanticAnalyzer& importTypeContext(json impAst);
	// Resolves the type or const `name` (as written in this module) if a
	// top-level declaration or import provides it and it is not resolved yet.
	// Through a pointer, a struct only has to exist.
	void ensureTypeName(const string& name, bool viaPointer);
	// ensureTypeName for every type and const name `node` refers to.
	// How a type reaches a struct it names: by its layout, or only by its name,
	// through a pointer or as what a `[+]T` or `[""]T` holds.
	enum class TypeReach { Layout, Pointer, Holder };
	void ensureTypeRefs(const json& node, TypeReach reach = TypeReach::Layout);
	// ensureTypeName for a struct by its program-wide name.
	void ensureStructDef(const string& name);
	void ensureInitElems(const json& structDef);
	void resolveTopDecl(const string& name, TopDecl& d);
	bool resolveTopConst(TopDecl& d);
	// The type the declaration of the top-level variable `name` gives it,
	// taken from the declaration's shape alone, or null.
	const json* topVarType(const string& name);
	// Resolves an array variable's declared type, filling the sizes its array
	// literal gives; false if a size is known only at run time.
	bool fixArrDeclSizes(const json& stmt, const json& var, json& vt);
	void rejectDeferredConstRefs(const json& locNode, const json& node) const;
	void adoptImportedType(ImportedType& t);
	// Runs `f` once no declaration is being resolved: resolving a pointee
	// earlier could report a cycle a pointer breaks.
	void whenSettled(function<void()> f);
	// Runs `f` once every type an allocator's shape may depend on is resolved.
	void whenShapesKnown(function<void()> f);
	// While one lives, types are being copied from another module.
	struct Adopting {
		TypeProgram& program;
		explicit Adopting(PlnSemanticAnalyzer& sa);
		~Adopting();
	};
	void adoptLaterPointee(PlnSemanticAnalyzer& sub, const json& type, const json& locNode);
	// A struct that may be named before its layout is resolved.
	bool awaitsLayout(const string& name) const { return declaredStructs_.count(name) && !structDefs_.count(name); }
	void adoptExportedTypes(const json& stmt, PlnSemanticAnalyzer& sub);
	void adoptExportedType(const json& stmt, const json& decl, PlnSemanticAnalyzer& sub);
	void adoptStruct(PlnSemanticAnalyzer& sub, const string& name, const string& written, const string& alias,
	                 const json& locNode);
	void adoptEnum(const PlnSemanticAnalyzer& sub, const string& name, const string& written, const string& alias,
	               const json& locNode);
	void adoptTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode);
	// adoptTypeDeps for the type of a variable or an owned field.
	void adoptVarTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode);
	void setTypeNameable(const string& name, bool nameable, const string& alias);
	// Records `name` as defined by `origin`; false if it already was, exits if
	// another module or a cincluded header defined it.
	bool claimTypeName(const string& name, const string& origin);
	void requireNotPalanType(const string& name, const json& locNode) const;
	bool namesPalanType(const json& binding) const;  // a binding in typeAliases_
	// Binds a type an import makes writable; exits if the name is bound to another type.
	void bindImportedTypeName(const json& locNode, const string& name, const json& type);
	// Exits if `written` names a Palan type this scope cannot write.
	void requireWritableTypeName(const json& locNode, const string& written) const;
	void sa_import(const json &stmt);
	PlnSemanticAnalyzer& importModule(const json& stmt, const json& imp_ast);
	void sa_cinclude(const json &stmt);
	void registerCIncludeTypes(const json& stmt); // cinclude structs/typedefs only
	json foldConstExpr(const json& expr);
	optional<ConstVal> evalConst(const json& expr);
	const json* lenOperand(const json& expr);
	optional<int64_t> constArrLen(const json& expr);
	json runtimeArrLen(const json& expr);
	int64_t constArrSizeOrExit(const json& arrType, const json& locNode);
	json arrLenOf(const json& saArr, const json& locNode);
	json bindRunLens(const string& name, const json& location);
	json sa_expression(const json &expr, const PlnType* expectedType = nullptr);
	// Also gives back a void call or a multi-value call, which has no value-type.
	json sa_expr_maybe_void(const json &expr, const PlnType* expectedType = nullptr);
	json sa_expr_arith(const json& expr, const PlnType* expectedType);
	json keyByName(const json& keyAst);
	CallArg subscriptContainer(const json& locNode, const json& ast, json saBase, bool forWrite);
	json sa_subscript_call(const json& expr, json saBase, const vector<Callee>& all, bool forWrite);
	json sa_operator_call(const json& expr, const string& op,
	                      const vector<const json*>& operands, vector<json> probes);
	json sa_expr_shift(const json& expr, const PlnType* expectedType);
	json sa_expr_append(const json& expr);
	json sa_expr_cmp(const json& expr);
	json sa_compare_call(const json& expr, json probes[2]);
	void readHexLit(json& lit);
	void checkIntLiteralRange(const json& lit);
	void checkBitFieldLiteral(const FieldLayout& field, const json& value);
	void rejectWideCharLit(const json& lit, const json& type);
	json sa_expr_call(const json& expr);
	json sa_func_ref(const json& expr, const json* toType = nullptr);
	// sa_expression given to toType, which chooses among functions of a name.
	json sa_expression_for(const json& expr, const json& toType);
	vector<Callee> funcsOfType(const string& name, const json& funcPtr, vector<Callee>* mismatched = nullptr);
	string funcRefType(const Callee& callee, json& funcPtr) const;
	bool namesOverloadedFunc(const json& expr) const;
	json literalDefaultType(const json& expr);
	bool isPureOperand(const json& expr);
	const json* findFuncPtrVar(const json& call) const;
	json funcPtrField(const json& memberCall);
	json sa_expr_call_ptr(const json& expr, json callee, const string& funcName);
	json sa_expr_member_call(const json& expr);
	json normalizeMethodCall(const json& expr);
	bool isMethodForm(const json& memberCall) const;
	const json* findCalleeByName(const string& name, bool& isCFunc) const;
	const json* findCallee(const json& call, bool& isCFunc) const;
	json operandValueType(const json& obj);
	json chainArg(const json& value, const json* paramVT, bool isCFunc, const json* valueType = nullptr);
	size_t chainRetCount(const json& value) const;
	json chainTarget(const json& call, size_t& at);
	json chainInto(json call, size_t at, const vector<json>& values);
	json chainValueAhead(const json& value, const json* paramVT, json& body);
	json normalizeChainCall(const json& expr);
	json chainAheadCall(const json& chain, json& body);
	json sa_expr_chain_call(const json& expr);
	json tappleCallExpr(const json& stmt);
	// Analyze a call's argument list against the callee's parameter list.
	json saCallArgs(const json& locNode, const json& args, const json& funcParams,
	                bool isCFunc, const string& funcName);
	bool takesExpectedType(const json& expr);
	json knownValueType(const json& expr);
	vector<CallArg> analyzeCallArgs(const json& args);
	optional<SaDiag> callArgMismatch(const json& argAst, const json& saArg, const json& param,
	                                 size_t argIdx, bool isCFunc, const string& funcName);
	json ownedBlockArg(const json& locNode, const json& arg, CallArg& ca, const json& param,
	                   size_t argIdx, const string& funcName);
	json variadicArrayArg(const json& locNode, const json& elemVT, json elems);
	CallArg spreadCallArg(const json& rawArg, bool last);
	optional<SaDiag> spreadMismatch(const json& vt, const json& elemVT) const;
	void checkSpreadPlace(const json& locNode, const json& args, const json& funcParams, const string& funcName);
	json spreadArrayArg(const json& locNode, CallArg& ca, const json& elemVT, json& count);
	json bindCallArgs(const json& locNode, vector<CallArg>& args, const json& funcParams,
	                  bool isCFunc, const string& funcName);
	[[noreturn]] void reportDiag(const json& locNode, const SaDiag& diag) const;
	optional<SaDiag> ptrPermissionMismatch(const json& from, const json& to);
	void checkPtrPermission(const json& locNode, const json& from, const json& to);
	// Shared narrowing rule for every binding site.
	json convertForBinding(const json& locNode, json value, const PlnType* toType, const json& toTypeJson);
	optional<SaDiag> callArgConvMismatch(const json& saArg, const json& paramVT);
	json convertCallArg(json saArg, const json& paramVT);
	// forWrite: the result is written through (store, '@!', or a field write
	// below it), so every hop back to the chain's root must be writable --
	// the same rule resolveObjectChain applies to struct field chains.
	json sa_index_base(const json& arr, bool forWrite);
	json sa_expr_arr_index(const json& expr, bool forWrite = false, json saBase = json());
	json sa_expr_field_access(const json& expr, bool forWrite);
	// Evaluate an array-size sub-expression (an `[n]T` declaration's `n`, or
	// a 2D array's inner/outer dimension) and normalize the result to
	// uint64, diagnosing E_ArraySizeNotInteger for a non-integer. Every
	// caller embeds the result directly into a hand-built uint64-typed node
	// (byte-count math, or a uint64 temp var's init) without further type
	// checking, so this is the one place that must guarantee uint64 --
	// sa_expression's expectedType alone no longer does, now that a
	// macro-folded lit-int keeps its own C-declared type instead of taking
	// expectedType unconditionally.
	json sa_arr_size_expr(const json& stmt, const json& sizeExprAst);
	json sa_expression_stmt(const json& stmt);
	json discardedValueStmt(const json& stmt, json body);
	json sa_var_decl(const json& stmt);           // returns array of statements
	// sa_var_decl for var-types already resolved (absent storage = heap).
	json sa_resolved_var_decl(const json& stmt, vector<VarStorage> storage = {});
	// all vars share one var-type; returns array of statements
	json sa_var_decl_group(const json& stmt, VarStorage storage);
	json sa_arr_var_decl(const json& stmt, VarStorage storage);       // returns array of statements
	json sa_arr_var_decl(const json& stmt, VarStorage storage, const Storage& st, bool litLen,
	                     const json* taken = nullptr);
	Storage arrVarStorage(const json& locNode, const json& vtype);
	AggregatePlace arrPlace(const json& self, const vector<json>& lens, bool litLen);
	json structAllocExpr(const string& name, bool build = true);
	// A struct a call returns is written into storage its receiver provides.
	void markStructRet(json& call, const json& locNode);
	json bindStructRet(json call, json dest);
	json declareStructRet(const string& name, const json& type, json call, VarStorage storage);
	json frameStructRelease(const string& name, const json& type);
	json structInitStmt(const string& name, const json& type);
	json returnIntoSlot(const json& stmt, json& ret, const string& returned);
	json heapStructRet(json value, json& pre);
	json sa_arr_lit_var_decl(const json& stmt, VarStorage storage);   // single var; returns array of statements
	json sa_arr_copy_var_decl(const json& stmt, VarStorage storage);  // single var; returns array of statements
	json sa_struct_lit_var_decl(const json& stmt, VarStorage storage); // single var; returns array of statements
	json sa_dict_lit_var_decl(const json& stmt, VarStorage storage);   // single var; returns array of statements
	// A dictionary literal's keys as `lit-str`s, in the order written.
	json dictLitKeys(const json& lit);
	json placeAggregateVar(const json& stmt, const string& name, const json& varType,
	                       json heapInit, json heapRelease, VarStorage storage,
	                       int64_t frameBytes, int frameAlign, json& post,
	                       optional<AllocShapeRef> shape = nullopt);
	json sa_struct_lit(const json& item, const StructDef& def);
	json sa_field_lit_value(const string& structName, const FieldLayout& f, const json& v);
	void emitStructLitAssigns(const json& objAst, const StructDef& def, const json& values, json& out);
	string arrLitDimSize(const json& stmt, const string& name, json& sizeExpr, size_t count);
	string fillArrLitSizes(const json& stmt, const string& name, json& varType, const json& items);
	json   strLitAsBytes(const json& value, const json& astType, const string& name);
	json   withStrLitBytes(const json& stmt, const json& targetAst, const json& targetType);
	// Where an array literal's elements go: through its levels, each of a
	// fixed size or a `[+]T` (size -1, varr its type) as long as its row, to
	// numeric leaf values or structs (structDef set) given as struct literals.
	// A level of a `[+]T` or a `[""]T` has no size, but its type; a dictionary's
	// literal gives its values under keys.
	struct ArrLitLevel { int64_t size; json varr; json dict; };
	struct ArrLitShape { vector<ArrLitLevel> levels; json leaf; const StructDef* structDef; };
	// varrTop: saType itself may be a `[+]T` or a `[""]T`, the first level.
	ArrLitShape arrLitShape(const json& locNode, const string& name, const json& saType, bool varrTop = false);
	json sa_arr_lit_values(const json& lit, const ArrLitShape& shape, const string& name, size_t depth = 0);
	void checkArrLitCount(const json& locNode, const json& items, const string& name,
	                      const ArrLitShape& shape, size_t depth = 0);
	// sized: a `[+]T` array already has the literal's length, as a declared
	// variable does; otherwise it is resized to it first.
	void emitArrLitElem(const json& elemAst, const ArrLitShape& shape, const json& value, json& out, size_t depth);
	void emitArrLitAssigns(const json& arrAst, const ArrLitShape& shape, const json& values, json& out,
	                       bool sized = true, size_t depth = 0);
	bool staticArrLitData(json& decl, const string& name, const ArrLitShape& shape, const json& values);
	json allocElemDesc(const json& elem, bool inPlace = false);
	json layoutElem(const json& elem);
	// The generated helpers of a fixed array of owned elements elem, which take
	// the array's length and then those of elem's levels, and of the `[+]T` vt.
	AllocShapeRef arrShape(const json& elem) { return {AllocFamily::Arr, layoutElem(elem)}; }
	AllocShapeRef varrShape(const json& vt);
	AllocShapeRef dictShape(const json& vt);
	void recordArrShape(const json& elem);
	void recordVarrShape(const json& vt);
	void recordDictShape(const json& vt);
	json dictValueAddr(json saDict, const json& dictAst, const json& keyAst, const string& op, const json& locNode);
	// The symbol of one of the functions build-mgr generates, and a call to it.
	string allocHelperSymbol(const AllocShapeRef& shape, const string& op) const;
	json allocHelperCall(const AllocShapeRef& shape, const string& op, json args) const;
	void recordElemShape(const AllocShapeRef& shape);
	json varrData(json varr);
	json valuesDataType(const json& vt, bool readOnly);
	void enterBlockDecls(const json& stmts);
	void declareTypeIdentity(const json& stmt, int seq);
	string topTypeIdentity(const string& written) const;
	void leaveBlockDecls();
	string declaredTypeName(const string& written) const;
	bool isDeclNameTaken(const string& name) const;
	bool isWrittenNameTaken(const string& name) const;  // isDeclNameTaken but for topDecls_
	static void addDeclaredVarNames(const json& stmt, set<string>& names);
	// Reported before its field types are resolved, where it would be a cycle.
	void rejectSelfEmbedding(const json& structDef) const;
	json sa_struct_def(const json& stmt);         // consume struct-def, register in structDefs_
	json sa_union_def(const json& stmt);
	bool builtWhole(const string& structName) const;
	void resolveFieldTypes(const json& stmt, json& fields);
	void setFieldInits(StructDef& def, const json& fields);
	json initRunCall(const string& structName, json run, json count);
	StructDef buildStructDef(const string& name, const json& fields, const json& locNode,
	                         bool isUnion = false, bool packed = false, int aligned = 1);
	int64_t fieldArrSize(const json& locNode, const json& sizeExpr);
	json ownedFieldType(const json& locNode, const json& vtype);
	FieldLayout inlineArrField(const json& locNode, const json& arr);
	void registerCStruct(const json& cincludeStmt, const json& s);  // consume c2ast "structs" entry, register in structDefs_
	json sa_struct_var_decl(const json& stmt, VarStorage storage); // returns array of statements
	json sa_type_alias(const json& stmt);         // consume type-alias, register in typeAliases_
	json sa_opaque_def(const json& stmt);
	void foldTypeSizes(json& type);
	json sa_const_decl(const json& stmt);         // consume const-decl, register in constDecls_
	json sa_enum_def(const json& stmt);           // consume enum-def, register in enumDefs_/typeAliases_
	// `written` is how source names it, `name` its program-wide name.
	void registerEnum(const string& name, map<string, int64_t> values, const json& loc, const string& written);
	void registerCEnum(const json& cincludeStmt, const json& e);
	void resolveCTypeRefs(json& node) const;      // C enum/earlier-cinclude typedef references -> SA type
	json enumTypeNamed(const string& name) const; // enum value-type for a type name, or null
	string typeNameWritten(const json& obj) const;
	json aliasCast(const json& expr, const string& typeName);
	json sa_expr_cast(const json& expr, bool moving);
	json castLens(const json& expr, const json& written, json view);
	// The source of a move, which alone may be an array cast to other sizes.
	json sa_move_source(const json& ast, const PlnType* expectedType = nullptr);
	// The lengths of the levels of the array `value` has, outermost first.
	json valueLens(const json& value);
	void checkMoveShape(const json& locNode, const json& value, const json& to, bool inherits);
	json inheritRunLens(const string& name, const json& varType, const json& value);
	json minLen(const json& a, const json& b, json& body);
	json copyRows(const json& locNode, const json& dst, const json& dstLens,
	              const json& src, const json& srcLens, const json& count, CopyInto into);
	json resolveEnumerator(const json& expr);     // `Name.X` as a lit-int, or null for a field access
	vector<string> enumeratorSpellings(const string& name) const;
	int64_t typeByteSize(const json& locNode, json type);  // `sizeof(type)`, type resolved
	void recordAllocShape(const string& structName);
	// Scope-exit release of an owned struct variable (pntrType is pntr(struct)).
	json makeOwnedFreeStmt(const string& name, const json& pntrType);
	json makeOwnedValueFreeStmt(json value, json lens = json());
	bool isStructType(const json& type) const;
	// True if the program-wide name `name` is a primitive or a registered struct.
	bool isKnownTypeName(const string& name) const;
	// isKnownTypeName plus "void", valid only as a pointer pointee, never a standalone value type.
	bool isKnownPointeeTypeName(const string& name) const;
	// Exits with E_UnknownType if a name at the leaf of `type` (through arr/pntr levels) is unknown,
	// or if a `$` element is not a struct or a `$[m]` row. Struct fields don't pass through here:
	// a `[n]$int32` field is an inline array, which a variable has no counterpart of.
	// `type` must be resolved (resolveWrittenType).
	void requireKnownTypeNames(const json& locNode, const json& type) const;
	// Exits unless each parameter/return of a function type is one a function
	// pointer can carry (isFuncSigType) and names known types.
	void requireFuncSigTypes(const json& locNode, const json& func) const;
	json  toStructPntrType(const json& type) const;
	json  unsizedArrToPntr(const json& locNode, const json& type);
	// A '@T'/'@!T' array element (a pointer slot) as SA represents it.
	json  ptrSlotElemType(const json& locNode, const json& type);
	void  rejectSizedArrBorrow(const json& locNode, const json& pntr) const;
	// A function type's parameters and return take sigType's form, so a
	// function pointer passes and returns owned values as a function does.
	json  deepNormalizePrimToStruct(const json& locNode, const json& type);
	json  fillKeptTypes(const json& locNode, json target, const json& from, const json& srcType);
	json  ownedCastType(const json& locNode, const json& written);
	// The type a written name is bound to in this scope, or null.
	const json* findTypeBinding(const string& written) const;
	// A type as written in source -> the same type in program-wide names, at
	// every level. Each written type passes through here exactly once.
	json  resolveWrittenType(const json& locNode, const json& type) const;
	// resolveWrittenType, then requireKnownTypeNames.
	json  resolveKnownType(const json& locNode, const json& type) const;
	// A function's parameter/return types resolved, before any normalization.
	json  resolveSigTypes(const json& f) const;
	// The canonical form of a signature type (an owned struct becomes pntr(struct)),
	// also what a tapple-decl variable's declared type is compared in.
	json  normalizeSigType(const json& locNode, const json& type);
	json  sigType(const json& locNode, const json& type);
	void  normalizeSig(json& funcDef);
	void  validateOperatorSig(const json& funcDef);
	void  validateSubscriptSig(const json& funcDef);
	void  registerCopyFunc(json& funcEntry, const string& scope);
	void  registerReleaseFunc(const json& funcEntry, const string& scope);
	void  emitReleaseFuncs();
	void  emitFieldInitFuncs();
	void  validateVariadicSig(const json& funcDef);
	void  validateCExportSig(const json& funcDef);
	json  normalizeArrBorrowType(const json& locNode, const json& type);
	json  ptrFieldType(const json& locNode, const json& pntr);
	json  varrBorrowType(const json& locNode, const json& arr, bool isMutable);
	json  valueElemType(const json& locNode, const json& holder);
	json  varrStorageType(const json& locNode, const json& arr);
	json  dictStorageType(const json& locNode, const json& dict);
	json  varrElemType(const json& locNode, const json& varr);
	json  takeOverType(const json& locNode, const json& varType);
	// The owned block a variable of `varType` holds (a struct's or a
	// constant-size array's), or null for any other type.
	json  ownedTypeOf(const json& locNode, const json& varType);
	int64_t inPlaceSize(const json& elem) const;
	json  inPlaceSizeExpr(const json& elem, size_t level, const function<json(size_t)>& lenAt) const;
	int   inPlaceAlign(const json& elem) const;
	// isMutable: the borrow's permission, or none for an owned level.
	json  sizedArrLevel(const json& locNode, const json& arr, optional<bool> isMutable,
	                    vector<json>* lens = nullptr);
	int64_t constLevelSize(const json& locNode, const json& sizeExprAst, bool isBorrow);
	const json* firstNonConstName(const json& expr) const;
	void nonConstNames(const json& expr, vector<const json*>& ids) const;
	optional<SaDiag> elemShapeMismatch(const json& saValue, const json& dstType);
	void  checkElemShape(const json& locNode, const json& saValue, const json& dstType);
	optional<SaDiag> arrBorrowBindingMismatch(const json& srcAst, const json& saValue,
	                                          const json& dstType);
	void  checkArrBorrowBinding(const json& locNode, const json& srcAst, const json& saValue,
	                            const json& dstType);
	json  sa_call_borrow_arg(const json& arg);
	json  borrowExpiring(json saArg, bool isMutable);
	json  sa_lit_arg(const json& arg, const json* paramVT, const string& paramName,
	                 const string& funcName, json& argAst, json& body, json& release);
	json  bindCallTemp(json saArg);
	void  checkBorrowSource(const json& locNode, const json& saValue, const json& dstType);
	bool  locationPath(const json& e, vector<string>& path);
	void  checkRangeChange(const json& locNode, const json& locAst, RangeChange change);
	void  checkRangeBorrowBinding(const json& locNode, const json& srcAst, const json& dstType);
	void  registerTypeAliasChecked(const json& locNode, const string& aliasName, const json& resolved);
	void  registerTypedefAliasInType(const json& locNode, json& vtype);
	void  registerCFuncTypedefAliases(const json& locNode, json& funcEntry);
	// Shared function pre-registration sequence (normalize + validate + register)
	// used by top-level, block-local, and function-nested func-defs alike.
	void  preregisterFunc(const json& f, const string& scope);
	json  normalizeFuncSig(const json& f, const string& scope);
	void  normalizeParamDefaults(json& funcEntry);
	// Diagnose Linux syscall ABI constraints on a syscall declaration
	// (funcDef must already be normalizeSig'd) and fold its
	// "syscall-number" expression node into a plain JSON integer.
	void  validateSyscallDecl(json& funcDef);
	void  rejectNotImplSig(const json& f);
	json  makeCallNode(const json& expr, const Callee& callee, const string& writtenName,
	                   vector<CallArg>* analyzedArgs = nullptr);
	json sa_field_assign(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	json sa_field_store(const json& stmt, const FieldChain& chain, const FieldLayout& field, json* litAddr);
	json sa_variant_assign(const json& stmt, const FieldChain& chain, json* litAddr);
	void takeValueFirst(json& s, const json& toType, bool borrows, json& body, json& post);
	FieldChain resolveObjectChain(const json& obj, bool forWrite);
	void checkVariant(FieldChain& chain, const FieldLayout& fld);
	json unionAddr(const FieldChain& chain) const;
	json sa_expr_is(const json& expr);
	json isBindingTest(const json& expr, const StructDef& def, json saX);
	const StructDef& testedUnion(const json& x, bool writes, json& saX);
	void requireVariant(const StructDef& def, const string& variant, const json& locNode) const;
	const FieldLayout& boundContents(const StructDef& def, const string& variant, const json& bind) const;
	json holdTestedUnion(json saX, const json& x, json& decls, json& stores);
	void pinBoundVariant(const json& x, const string& ptr, const string& variant, const string& name);
	json sa_if_is_stmt(const json& stmt);
	const StructDef* unionNamed(const json& obj, string& written) const;
	json variantValue(const json& expr, bool isCall);
	json variantMakerDef(const StructDef& def, const string& variant) const;
	void emitVariantMakers();
	void requireOwnedBase(const json& saBase, const json& locNode);
	json makeFieldAssign(const FieldChain& chain, const FieldLayout& field, json value);
	json makeFieldAccess(const FieldChain& chain, const FieldLayout& field);
	const FieldLayout& findFieldOrExit(const string& structName, const string& fieldName, const json& locNode,
	                                   bool forWrite);
	bool outsideTypeModule(const string& structName) const;
	string structDisplayName(const string& structName) const;
	string typeDisplayName(const json& t) const;
	// Look up a struct already known to be registered (name presence must be checked
	// by the caller beforehand) and reject it if its layout isn't known yet.
	const StructDef& requireCompleteStruct(const string& structName, const json& locNode, const string& use = "this");
	// The opaque type a pointer (an owner or a borrow) points to, or null.
	const StructDef* opaqueDefOf(const json& vt) const;
	// A struct's own storage, which is passed and returned by value; an opaque
	// type's is only ever its owner, a pointer.
	bool isStructValue(const json& vt) const;
	// Whether this module can write the type of an opaque type's contents,
	// which lets it allocate, copy and convert them.
	bool opaqueContentsVisible(const StructDef& def) const;
	json asOpaqueContents(json value, const StructDef& def) const;
	bool holdsOpaque(const json& vt) const;
	bool holdsOpaque(const json& vt, set<string>& seen) const;
	json opaqueConversion(const json& expr, const json& src, const json& written);
	json sa_opaque_var_decl(const json& stmt, VarStorage storage);
	// `entry` must be a cinclude'd C function entry already processed by
	// normalizeCFuncSig -- the only producer of "_unsupported-sig". No-op if
	// the signature is fully representable.
	void requireSupportedCFuncSig(const json& entry, const string& funcName, const json& locNode);
	// `entry` must be a cinclude'd C global entry already processed by
	// normalizeCGlobal -- the only producer of "_unsupported-global". No-op if
	// the type is fully representable.
	void requireSupportedCGlobal(const json& entry, const string& globalName, const json& locNode);
	json sa_expr_addr_of(const json& expr);
	void sa_functions(const json& funcs);
	void sa_function(const json& funcDef, bool topLevel = false);
	json sa_store_scope(const json& stmt);  // returns json::array()
	json sa_store_stmt(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	void hoistStores(json& value, json& body);
	json sa_store_steps(const json& stmt, json& body, bool readBack);
	json storeTargetType(const json& stmt);
	json pinLocation(json loc, json& body);
	string pinTemp(json saValue, const json& locNode, json& body);
	json pinValue(json saValue, const json& astValue, json& body);
	bool valueFirst(json& s, json& pre, json& post, set<string>& roots);
	json snapshotCopy(const json& saSrc, const json& locNode, json& pre, json& post);
	json takeScopeRelease(const string& name);
	const json& scopeRelease(const string& name) const;
	void noteEscape(const json& saValue);
	bool storageRoots(const json& v, set<string>& roots, bool contents);
	void resolvePending(json& stmts);
	json sa_assign_stmt(const json& stmt);  // returns json::array()
	json sa_lit_assign_via_temp(const json& stmt, const json& varType, json* litAddr = nullptr);
	json sa_transfer_to_var(const json& stmt, const string& name, const json& varType, const PlnType* toType);
	json sa_transfer_to_owner_view(const json& stmt, const json& target);
	// `src -> dst` for a dst that isCopiedByValue: a statement copying src's
	// contents into dst's storage. Diagnoses E_CopyShapeMismatch.
	json elemsFiniStmt(const string& name, const json& arrType);
	json makeCopyStmt(const json& locNode, const json& dst, const json& src, CopyInto into);
	bool assignsByFunc(const json& t) const;
	json sa_subscript_put_stmt(const json& stmt, json saBase, const vector<Callee>& puts);
	json sa_arr_assign_stmt(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	json sa_dict_store_stmt(const json& stmt, json saDict, json* litAddr);
	// `[...] -> target` / `{...} -> target` for a struct or array target; returns a block.
	// With litAddr, returns the statements without a scope of their own and
	// sets litAddr to the target's address, held in a temp, to read it back.
	json sa_aggregate_lit_assign(const json& stmt, const json& targetAst, const json& targetType,
	                             json* litAddr = nullptr);
	void appendTransferSourceReset(json& stmts, const json& stmt, const json& saValue, const string& op = "->>");
	// What srcAst owns, given away by `op`: held in a temp declared in body, with
	// the owner reset to NULL after it. Typed as the owned value, category "expiring".
	json giveValue(const json& srcAst, const json& locNode, const string& op, json& body, json& reset);
	json sa_expr_transfer(const json& expr);
	json sa_arg_expression(const json& ast, const json* toType = nullptr);
	void checkGivenOwners(const json& locNode, const vector<json>& argAsts, const json& saArgs);
	bool isGlobalName(const string& name) const;
	// `T b <<= v`: the initializer as a pinned value b takes over; its evaluation goes to pre.
	json takeOverInit(const json& stmt, json& var, VarStorage storage, json& pre);
	// `value ->> target`: releases what target holds, stores, and resets the source.
	json transferStmts(const json& stmt, json targetRelease, json store, const json& value);
	// Rejects a returned `@x` of storage reached from a local variable through owners alone.
	void checkReturnedBorrow(const json& stmt, const json& valueAst);
	json sa_return_stmt(const json& stmt);
	json bindReturnValueToTemp(const json& stmt, json& ret);
	json bindReturnLit(json& stmt);
	json copyReturnBlock(const json& stmt, json& ret);
	void checkMultiRetCall(const json& stmt, const json& callExpr, size_t recvCount);
	json sa_tapple_decl(const json& stmt);       // returns array of statements
	json sa_resolved_tapple_decl(const json& stmt, const vector<VarStorage>& storages);
	json sa_tapple_assign(const json& stmt);
	json sa_block(const json& stmt);
	json sa_if_stmt(const json& stmt);
	void markIsBindings(json& cond, vector<pair<string, json>>& aliases);
	json closeIsScope(IsScope& scope, IsScope* outer, json stmt);
	void declareIsBound(const IsScope& scope);
	json sa_while_stmt(const json& stmt);
	json sa_loop_body(const json& astBody, const json& heads, json step);
	json sa_for_stmt(const json& stmt);
	json sa_for_array(const json& stmt, json range);
	void  holdCallTemps(json& node);
	json sa_for_struct(const json& stmt, const json& rangeVT, json* rangeSa);
	json forIndexHead(const json& stmt, const string& idx, const json* arrType);
	json sa_break_stmt(const json& stmt);
	json sa_continue_stmt(const json& stmt);

public:
	PlnSemanticAnalyzer(string c2ast_path);
	~PlnSemanticAnalyzer();
	void analysis(const json &ast, const string& astPath);
	const json& result();
	// The symbols of a .pai's public functions, as an importer of it resolves them.
	json interfaceSymbols(const json& ast);
	void setLibraries(map<string, string> libraries);  // import <pkg/mod> -> its AST file
	void setChecks(bool checks) { checks_ = checks; }
};
