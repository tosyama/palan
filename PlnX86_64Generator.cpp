#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <boost/format.hpp>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;
using std::ostringstream;
using boost::format;
using boost::algorithm::replace_all_copy;

enum GenEttyType {
	GE_STRING
};

void PlnGenEntity::freeEntity(PlnGenEntity* e)
{
	BOOST_ASSERT(e != NULL);
	switch (e->type) {
		GE_STRING:
			delete e->data.str;
		break;
	}
	delete e;
}

PlnX86_64Generator::PlnX86_64Generator(ostream& ostrm)
	: PlnGenerator(ostrm)
{
}

void PlnX86_64Generator::genSecReadOnlyData()
{
	os << ".section .rodata" << endl;
}

void PlnX86_64Generator::genSecText()
{
	os << ".text" << endl;
}

void PlnX86_64Generator::genEntryPoint(const string& entryname)
{
	os << ".global ";
	if (entryname == "main")
		os << "_start" << endl;
	else
		os << entryname << endl;
}

void PlnX86_64Generator::genLabel(const string& label)
{
	if (label == "main") os << "_start";
	else os << label;
	
	os << ":" << endl;
}

void PlnX86_64Generator::genSysCall(int id, vector<PlnGenEntity*>& args, const string& comment)
{
	static const char* regs[] = {"%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9"};
	BOOST_ASSERT(args.size() <= 6);

	for (int i=args.size()-1; i>=0; --i)
		os << format("	movq %1%, %2%") % *(args[i]->data.str) % regs[i] << endl;
		
	os << format("	movq $%1%, %%rax	# %2%") % id % comment << endl;
	os << "	syscall" << endl;
}

void PlnX86_64Generator::genStringData(int index, const string& str)
{
	os << format(".LC%1%:") % index << endl;
	string ostr = replace_all_copy(str,"\n","\\n");
	os << format("	.string \"%1%\"") % ostr << endl;
}

PlnGenEntity* PlnX86_64Generator::getInt(int i)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->data.str = new string((format("$%1%") % i).str());

	return e;
}

PlnGenEntity* PlnX86_64Generator::getStrAddress(int index)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->data.str = new string((format("$.LC%1%") % index).str());

	return e;
}
