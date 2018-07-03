#include <vector>

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
	bool operator==(const PlnScopeItem &si) const
	{
		return (type == si.type && inf.block == si.inf.block);
	}
};

typedef std::vector<PlnScopeItem>	PlnScopeStack;

// use for finishing
enum PlnVarLifetime {
	VLT_UNKNOWN,
	VLT_ALLOCED,
	VLT_INITED,
	VLT_FREED
};

class PlnScopeVarInfo {
public:	
	PlnVariable* var;
	PlnScopeItem scope;

	PlnVarLifetime lifetime;
	PlnScopeVarInfo(PlnVariable* v, PlnScopeItem s)
		:var(v), scope(s), lifetime(VLT_UNKNOWN) { };
};

class PlnScopeInfo {
public:
	vector<PlnScopeVarInfo> owner_vars;
	PlnScopeStack scope;
	void push_scope(PlnFunction* f) { scope.push_back(PlnScopeItem(f)); }
	void push_scope(PlnBlock* b) { scope.push_back(PlnScopeItem(b)); }
	void pop_scope() { scope.pop_back(); }
	void push_owner_var(PlnVariable* v) { owner_vars.push_back(PlnScopeVarInfo(v, scope.back())); }
	void pop_owner_vars(PlnBlock *b) {
		for (auto it=owner_vars.begin(); it!=owner_vars.end();) {
			if (it->scope.inf.block == b) it = owner_vars.erase(it);
			else ++it;
		}
	}
	void pop_owner_vars(PlnFunction *f) {
		for (auto it=owner_vars.begin(); it!=owner_vars.end();) {
			if (it->scope.inf.function == f) it = owner_vars.erase(it);
			else ++it;
		}
	}

	bool exists_current(PlnVariable* v);
	void set_lifetime(PlnVariable* v, PlnVarLifetime lt);
	PlnVarLifetime get_lifetime(PlnVariable* v);
};
