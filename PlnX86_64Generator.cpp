#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <boost/format.hpp>
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::endl;
using std::ostringstream;
using boost::format;

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
	os << format("	movq $%1%, %%rax	# %2%") % id % comment << endl;
	if (args.size()>=1)
		os << format("	movq %1%, %%rdi") % *(args[0]->data.str) << endl;
	os << "	syscall" << endl;
}

PlnGenEntity* PlnX86_64Generator::getInt(int i)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	ostringstream ss;
	ss << '$' << i;
	e->data.str = new string(ss.str());

	return e;
}
