#include "PlnModel.h"
#include "PlnX86_64Generator.h"

void PlnExpression::gen(PlnGenerator& g)
{
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_SYS:
			g.genSysCall(function->inf.syscall.id, function->name);
	}
}
