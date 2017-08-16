#include <boost/assert.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;
using std::to_string;
using boost::adaptors::reverse;

// PlnReturnPlace
string PlnReturnPlace::commentStr()
{
	switch (type) {
		case RP_ARGPLN:
		case RP_ARGSYS:
			return "arg" + to_string(inf.index);
		case RP_VAR:
			return inf.var->name;
	}
	return "";
}

void PlnReturnPlace::dump(ostream& os, string indent)
{
	os << indent << "ReturnPlace:";
	switch (type) {
		case RP_NULL: os << "Null"; break;
		case RP_VAR: os << "Variable"; break;
		case RP_AS_IS: os << "As is"; break;
		case RP_ARGPLN: os << "Palan Argument"; break;
		case RP_ARGSYS: os << "Syscall Argument"; break;
		case RP_WORK: os << "Work Area"; break;
		default:
			os << "Unknown" << to_string(type);
	}
	os << endl;
}

PlnGenEntity* PlnReturnPlace::genEntity(PlnGenerator& g)
{
	switch (type) {
		case RP_NULL: return g.getNull();
		case RP_VAR: return inf.var->genEntity(g);
		case RP_AS_IS: return inf.as_is->genEntity(g);
		case RP_ARGPLN: return g.getArgument(inf.index);
		case RP_ARGSYS: return g.getSysArgument(inf.index);
		case RP_WORK: return g.getWork(inf.index);
		default:
			BOOST_ASSERT(false);
	}
	return NULL;
}

// PlnExpression
PlnExpression::PlnExpression(PlnValue value)
	: type(ET_VALUE) 
{
	values.push_back(value);
}

void PlnExpression::finish()
{
	if (ret_places[0].type == RP_AS_IS) {
		ret_places[0].inf.as_is = &values[0];
	}
}

void PlnExpression::dump(ostream& os, string indent)
{
	if (type == ET_VALUE) {
		for (auto &value: values)
		switch (value.type) {
			case VL_LIT_INT8:
				os << indent << "Int literal: " << value.inf.intValue << endl;
				break;
			case VL_VAR:
				os << indent << "Variable: " << value.inf.var->name << endl;
				break;
			case VL_RO_DATA:
				os << indent << "String literal: " << value.inf.rod->name.size() << endl;
				break;
			default:
				BOOST_ASSERT(false);
		}
	} else 
		os << indent << "Expression: " << type << endl;
}

void PlnExpression::gen(PlnGenerator& g)
{
	for (int i=0; i<ret_places.size(); ++i) {
		PlnGenEntity* re = values[i].genEntity(g);
		PlnGenEntity* le = ret_places[i].genEntity(g);
		
		g.genMove(le, re, ret_places[i].commentStr());
		PlnGenEntity::freeEntity(re);
		PlnGenEntity::freeEntity(le);
	}
}

PlnMultiExpression::PlnMultiExpression()
	: PlnExpression(ET_MULTI)
{
}

PlnMultiExpression::PlnMultiExpression(
	PlnExpression* first, PlnExpression* second)
	: PlnExpression(ET_MULTI)
{
	values = first->values;
	exps.push_back(first);
	append(second);
}

void PlnMultiExpression::append(PlnExpression* exp)
{
	values.insert(values.end(), exp->values.begin(), exp->values.end());
	exps.push_back(exp);
}

void PlnMultiExpression::finish()
{
	int i=0;
	for (auto exp: exps) {
		for (auto v: exp->values) {
			exp->ret_places.push_back(ret_places[i]);
			exp->finish();
			i++;
		}
	}
}

void PlnMultiExpression::dump(ostream& os, string indent)
{
	os << indent << "MultiExpression: " << exps.size() << endl;
	for (auto e: exps)
		e->dump(os, indent+" ");
}

void PlnMultiExpression::gen(PlnGenerator& g)
{
	for (auto e: exps) {
		e->gen(g);
	}

	int i=0;
	for (auto exp: exps)
		for (auto rp: exp->ret_places) {
			PlnGenEntity* re = rp.genEntity(g);
			PlnGenEntity* le = ret_places[i].genEntity(g);
			g.genMove(le, re, ret_places[i].commentStr());
			PlnGenEntity::freeEntity(re);
			PlnGenEntity::freeEntity(le);
			i++;
		}
}

// PlnFunctionCall
PlnFunctionCall:: PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args)
	: PlnExpression(ET_FUNCCALL),
	function(f),
	arguments(move(args))
{
	int i=0;
	for (auto rv: f->return_vals) {
		PlnVariable* ret_var = new PlnVariable();
		ret_var->name = rv->name;
		ret_var->alloc_type = VA_RETVAL;
		ret_var->inf.index = i;

		values.push_back(PlnValue(ret_var));

		++i;
	}
}

void PlnFunctionCall::finish()
{
	PlnReturnPlace rp;
	switch (function->type) {
		case FT_PLN: rp.type = RP_ARGPLN; break;
		case FT_SYS: rp.type = RP_ARGSYS; break;
		case FT_C: rp.type = RP_ARGPLN; break;
		default:
			BOOST_ASSERT(false);
	}

	int i = function->return_vals.size();
	if (i==0) i=1;
	for (auto a: arguments) {
		rp.inf.index = i;
		a->ret_places.push_back(rp);
		a->finish();
		++i;
	}
}

void PlnFunctionCall:: dump(ostream& os, string indent)
{
	os << indent << "FunctionCall: " << function->name << endl;
	os << indent << " Arguments: " << arguments.size() << endl;
	for (auto a: arguments) {
		if (a) a->dump(os, indent + "  ");
		else os << indent + "  NULL" << endl;
	}
}

void PlnFunctionCall::gen(PlnGenerator &g)
{
	switch (function->type) {
		case FT_PLN:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genCCall(function->name);
			int i = 0;
			for (auto rp: ret_places) {
				PlnGenEntity* dst = rp.genEntity(g);
				PlnGenEntity* src = values[i].genEntity(g);

				g.genMove(dst, src, rp.commentStr());

				PlnGenEntity::freeEntity(dst);
				PlnGenEntity::freeEntity(src);
				++i;
			}
			break;
		}
		case FT_SYS:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genSysCall(function->inf.syscall.id, function->name);
			break;
		}
		case FT_C:
		{
			for (auto arg: reverse(arguments)) 
				arg->gen(g);
			g.genCCall(function->name);
			break;
		}
		default:
			BOOST_ASSERT(false);
	}
}

// PlnAddOperation
PlnExpression* PlnAddOperation::create(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 1+2 => 3
			l->values[0].inf.intValue += r->values[0].inf.intValue;
			delete r;
			return l;
		} else {
			// e.g.) 1+a => a+1
			PlnExpression *t;
			t = l;
			l = r;
			r = t;
		}
	} else if (l->type == ET_ADD) {
		PlnAddOperation* po = static_cast<PlnAddOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a+1+2 => a+3
					po->r->values[0].inf.intValue += r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}

	return new PlnAddOperation(l,r);
}

PlnExpression* PlnAddOperation::create_sub(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 1-2 => -1
			l->values[0].inf.intValue -= r->values[0].inf.intValue;
			delete r;
			return l;
		}
	} else if (l->type == ET_ADD) {
		PlnAddOperation* po = static_cast<PlnAddOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a+1-2 => a+(-1)
					po->r->values[0].inf.intValue -= r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}
	if (r->type == ET_VALUE) {
		if (r->values[0].type == VL_LIT_INT8) {
			// e.g.) a-1 => a+(-1)
			r->values[0].inf.intValue *= -1;
			return new PlnAddOperation(l,r);
		}
	}
	return new PlnAddOperation(l,r,false);
}

PlnAddOperation::PlnAddOperation(PlnExpression* l, PlnExpression* r, bool is_add)
	: PlnExpression(ET_ADD), l(l), r(r), is_add(is_add)
{
	PlnValue v;
	v.type = VL_WK_INT8;
	values.push_back(v);
}

void PlnAddOperation::finish()
{
	BOOST_ASSERT(ret_places.size()==1);
	int index = 0;
	if (ret_places[0].type == RP_WORK) {
		index = ret_places[0].inf.index;
	}
	PlnReturnPlace rp;
	rp.type = RP_WORK;
	rp.inf.index = index;
	l->ret_places.push_back(rp);
	l->finish();

	if (r->type == ET_VALUE)
		rp.type = RP_AS_IS;
	else
		rp.inf.index = index+1;
	r->ret_places.push_back(rp);
	r->finish();
}

void PlnAddOperation::dump(ostream& os, string indent)
{
	if (is_add) os << indent << "ADD" << endl;
	else os << indent << "SUB" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnAddOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	PlnGenEntity* le = l->ret_places[0].genEntity(g);
	PlnGenEntity* re = r->ret_places[0].genEntity(g);
	PlnGenEntity* rpe = ret_places[0].genEntity(g);
	if (is_add) g.genAdd(le, re);
	else g.genSub(le, re);
	g.genMove(rpe, le, ret_places[0].commentStr());

	PlnGenEntity::freeEntity(le);
	PlnGenEntity::freeEntity(re);
}

// PlnNegative
PlnExpression* PlnNegative::create(PlnExpression *e)
{
	if (e->type == ET_VALUE && e->values[0].type == VL_LIT_INT8) {
		e->values[0].inf.intValue = - e->values[0].inf.intValue;
		return e;
	}
	return new PlnNegative(e); 
}

PlnNegative::PlnNegative(PlnExpression* e)
	: PlnExpression(ET_NEG), e(e)
{
	PlnValue v;
	v.type = VL_WK_INT8;
	values.push_back(v);
}

void PlnNegative::finish()
{
	int index = 0;
	if (ret_places[0].type == RP_WORK) {
		index = ret_places[0].inf.index;
	}
	PlnReturnPlace rp;
	rp.type = RP_WORK;
	rp.inf.index = index;
	e->ret_places.push_back(rp);
	e->finish();
}

void PlnNegative::dump(ostream& os, string indent)
{
	os << indent << "Negative:" << endl;
	e->dump(os, indent+" ");
}

void PlnNegative::gen(PlnGenerator& g)
{
	e->gen(g);

	PlnGenEntity* ne = e->ret_places[0].genEntity(g);
	PlnGenEntity* rpe = ret_places[0].genEntity(g);

	g.genNegative(ne);
	g.genMove(rpe, ne, ret_places[0].commentStr());

	PlnGenEntity::freeEntity(ne);
}

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, PlnExpression* exp)
	: PlnExpression(ET_ASSIGN), expression(exp)
{
	values = move(lvals);
}

void PlnAssignment::finish()
{
	PlnReturnPlace rp;
	rp.type = RP_VAR;
	for (auto lv: values) {
		rp.inf.var = lv.inf.var;
		expression->ret_places.push_back(rp);
	}
	expression->finish();
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	expression->dump(os, indent+" ");	
}

void PlnAssignment::gen(PlnGenerator& g)
{
	expression->gen(g);
	PlnExpression::gen(g);
}

