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
enum GenEttyAllocType {
	GA_NULL,
	GA_CODE,
	GA_REG,
	GA_MEM
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

void PlnX86_64Generator::genEntryFunc()
{
	os << "	pushq %rbp" << endl;
	os << "	movq %rsp, %rbp" << endl;
}

void PlnX86_64Generator::genLocalVarArea(int size)
{
	if (size)
		os << format("	subq $%1%, %%rsp") % size << endl;
}

void PlnX86_64Generator::genCCall(string& cfuncname)
{
	os << "	xorq %rax, %rax" << endl;
	os << "	call " << cfuncname << endl;
}

void PlnX86_64Generator::genSysCall(int id, const string& comment)
{
	os << format("	movq $%1%, %%rax	# %2%") % id % comment << endl;
	os << "	syscall" << endl;
}

void PlnX86_64Generator::genReturn()
{
	os << "	movq %rbp, %rsp" << endl;
	os << "	popq %rbp" << endl;
	os << "	ret" << endl;
}

void PlnX86_64Generator::genMainReturn(vector<PlnGenEntity*> &return_vals)
{
	os << "	movq %rbp, %rsp" << endl;
	os << "	popq %rbp" << endl;
	if (return_vals.size() > 0)
		os << format("	movq %1%, %%rdi") % *(return_vals[0]->data.str) << endl;
	else
		os << "	xorq %rdi, %rdi" << endl;
	os << "	movq $60, %rax" << endl;
	os << "	syscall"<< endl;
}

void PlnX86_64Generator::genStringData(int index, const string& str)
{
	os << format(".LC%1%:") % index << endl;
	string ostr = replace_all_copy(str,"\n","\\n");
	os << format("	.string \"%1%\"") % ostr << endl;
}

void PlnX86_64Generator::genMove(PlnGenEntity* dst, PlnGenEntity* src, string comment)
{
	if (dst->alloc_type == src->alloc_type) {
		if(*dst->data.str == *src->data.str) return;	// do nothing
	}

	if (dst->alloc_type == GA_NULL) return;

	if (dst->alloc_type != GA_MEM || src->alloc_type != GA_MEM) {
		os << format("	movq %1%, %2%") % *src->data.str % *dst->data.str;
	} else {
		os << format("	movq %1%, %%rax") % *src->data.str << endl;
		os << format("	movq %%rax, %1%") % *dst->data.str;
	}
	if (comment != "") os << "	# " << comment;
	os << endl;
}

void PlnX86_64Generator::genAdd(PlnGenEntity* dst, PlnGenEntity* src)
{
	BOOST_ASSERT(dst->alloc_type != GA_MEM || src->alloc_type != GA_MEM);
	if (src->alloc_type == GA_CODE && *src->data.str == "$1") {
		os << "	incq " << *dst->data.str << endl;
		return;
	}
	os << format("	addq %1%, %2%") % *src->data.str % *dst->data.str << endl;
}

PlnGenEntity* PlnX86_64Generator::getNull()
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_NULL;
	e->data.str = new string("$0");

	return e;
}

PlnGenEntity* PlnX86_64Generator::getInt(int i)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_CODE;
	e->data.str = new string((format("$%1%") % i).str());

	return e;
}

PlnGenEntity* PlnX86_64Generator::getStackAddress(int offset)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_MEM;
	e->data.str = new string((format("-%1%(%%rbp)") % offset).str());

	return e;
}

PlnGenEntity* PlnX86_64Generator::getStrAddress(int index)
{
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_MEM;
	e->data.str = new string((format("$.LC%1%") % index).str());

	return e;
}

PlnGenEntity* PlnX86_64Generator::getArgument(int i)
{
	BOOST_ASSERT(i>=0 && i <= 6);
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	switch (i) {
		case 0: e->data.str = new string("%rax");	break;
		case 1: e->data.str = new string("%rdi");	break;
		case 2: e->data.str = new string("%rsi");	break;
		case 3: e->data.str = new string("%rdx");	break;
		case 4: e->data.str = new string("%rcx");	break;
		case 5: e->data.str = new string("%r8");	break;
		case 6: e->data.str = new string("%r9");	break;
	}
	return e;
}

PlnGenEntity* PlnX86_64Generator::getSysArgument(int i)
{
	BOOST_ASSERT(i>=0 && i <= 6);
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	switch (i) {
		case 0: e->data.str = new string("%rax");	break;
		case 1: e->data.str = new string("%rdi");	break;
		case 2: e->data.str = new string("%rsi");	break;
		case 3: e->data.str = new string("%rdx");	break;
		case 4: e->data.str = new string("%r10");	break;
		case 5: e->data.str = new string("%r8");	break;
		case 6: e->data.str = new string("%r9");	break;
	}
	return e;
}

PlnGenEntity* PlnX86_64Generator::getWork(int i)
{
	BOOST_ASSERT(i>=0 && i <= 2);
	PlnGenEntity* e= new PlnGenEntity();
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	switch (i) {
		case 0: e->data.str = new string("%rax");	break;
		case 1: e->data.str = new string("%rdi");	break;
		case 2: e->data.str = new string("%rsi");	break;
	}
	return e;
}
