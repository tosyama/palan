#include <vector>
#include <algorithm>

class PlnModule;
class PlnFunction;
class PlnBlock;
class PlnVariable;

enum PlnScpType {
	SC_MODULE,
	SC_FUNCTION,
	SC_BLOCK
};

// use for parse.
class PlnScopeItem
{
public:
	PlnScopeItem(PlnModule* m) : type(SC_MODULE) { inf.module = m; }
	PlnScopeItem(PlnFunction* f) : type(SC_FUNCTION) { inf.function = f; }
	PlnScopeItem(PlnBlock* b) : type(SC_BLOCK) { inf.block = b; }
	PlnScpType type;
	union {
		PlnModule *module;
		PlnFunction *function;
		PlnBlock *block;
	} inf;
};

typedef std::vector<PlnScopeItem>	PlnScopeStack;

inline PlnFunction* searchFunction(PlnScopeStack& ss)
{
	auto itm = std::find_if(ss.rbegin(), ss.rend(), [](PlnScopeItem& i) { return i.type == SC_FUNCTION; });
	if (itm != ss.rend()) return itm->inf.function;
	else return NULL;
}

// use for finishing
class PlnScopeVarInfo {
public:	
	PlnVariable* var;
	PlnScopeItem scope;

	PlnScopeVarInfo(PlnVariable* v, PlnScopeItem s) :var(v), scope(s) { };
};

class PlnScopeInfo {
public:
	vector<PlnScopeVarInfo> owner_vars;
	PlnScopeStack scope;
	void push(PlnBlock* b) { scope.push_back(PlnScopeItem(b)); }
	void pop() { scope.pop_back(); }
	void push_owner_var(PlnVariable* v) { owner_vars.push_back(PlnScopeVarInfo(v, scope.back())); }
	void pop_owner_vars(PlnBlock *b) {
		for (auto it=owner_vars.begin(); it!=owner_vars.end();) {
			if (it->scope.inf.block == b) it = owner_vars.erase(it);
			else ++it;
		}
	}
};
