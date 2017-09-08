#include <vector>
#include <algorithm>

class PlnModule;
class PlnFunction;
class PlnBlock;

enum PlnScpType {
	SC_MODULE,
	SC_FUNCTION,
	SC_BLOCK
};

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
