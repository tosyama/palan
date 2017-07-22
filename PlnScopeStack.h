#include <vector>

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

