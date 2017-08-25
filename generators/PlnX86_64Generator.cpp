/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <boost/format.hpp>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "../PlnModel.h"
#include "PlnX86_64Generator.h"

using std::ostringstream;
using boost::format;
using boost::algorithm::replace_all_copy;

enum GenEttyAllocType {
	GA_NULL,
	GA_CODE,
	GA_REG,
	GA_MEM
};

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
	if (size) {
		if (size % 16)
			size = 16 * (size / 16 + 1);
		os << format("	subq $%1%, %%rsp") % size << endl;
	}
}

void PlnX86_64Generator::genFreeLocalVarArea(int size)
{
	if (size) {
		if (size % 16)
			size = 16 * (size / 16 + 1);
		os << format("	addq $%1%, %%rsp") % size << endl;
	}
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

void PlnX86_64Generator::genMainReturn()
{
	os << "	movq %rbp, %rsp" << endl;
	os << "	popq %rbp" << endl;
	os << "	movq $60, %rax" << endl;
	os << "	syscall"<< endl;
}

void PlnX86_64Generator::genStringData(int index, const string& str)
{
	os << format(".LC%1%:") % index << endl;
	string ostr = replace_all_copy(str,"\n","\\n");
	os << format("	.string \"%1%\"") % ostr << endl;
}

static string resizeReg(string* reg, int size)
{
	if (*reg == "%rax") 
		switch(size) {
			case 1: return "%al";
			case 2: return "%ax";
			case 4: return "%eax";
		}
	BOOST_ASSERT(false);
}

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	string dst_safix = "q";
	string src_safix = "";

	if (dst->alloc_type == GA_MEM) {
		switch (dst->size) {
			case 1: dst_safix = "b"; break;
			case 2: dst_safix = "w"; break;
			case 4: dst_safix = "l"; break;
		}
		if (src->alloc_type == GA_REG && dst->size<=4)
			*src->data.str = resizeReg(src->data.str, dst->size);
	}

	if (src->alloc_type == GA_MEM) {
		if (dst->size > src->size)
			switch (src->size) {
				case 1: src_safix = "zb"; break;
				case 2: src_safix = "zw"; break;
				case 4: src_safix = "zl"; break;
			}
	}

	if (dst->alloc_type == src->alloc_type) {
		if(*dst->data.str == *src->data.str) return;	// do nothing
	}

	if (dst->alloc_type == GA_NULL) return;

	if (dst->alloc_type != GA_MEM || src->alloc_type != GA_MEM) {
		os << "	mov" << src_safix << dst_safix << " " << *src->data.str << ", " << *dst->data.str;
	} else {
		os << "	mov" << src_safix << dst_safix << " " << *src->data.str << ", %rax" << endl;
		os << "	mov" << dst_safix << " %rax, " <<  *dst->data.str;
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


	// TODO: templary imprement. need to refacter.
	PlnGenEntity work;
	if (src->alloc_type == GA_MEM) {
		if (src->size < 8) {
			work.type = GA_REG;
			work.size = 8;
			work.data.str = new string("%r11");
			genMove(&work, src, "");
			src = &work;
		}
	}
		
	os << "	addq " << *src->data.str << ", " << *dst->data.str << endl;
}

void PlnX86_64Generator::genSub(PlnGenEntity* dst, PlnGenEntity* src)
{
	BOOST_ASSERT(dst->alloc_type != GA_MEM || src->alloc_type != GA_MEM);
	if (src->alloc_type == GA_CODE && *src->data.str == "$1") {
		os << "	decq " << *dst->data.str << endl;
		return;
	}
	// TODO: templary imprement. need to refacter.
	PlnGenEntity work;
	if (src->alloc_type == GA_MEM) {
		if (src->size < 8) {
			work.type = GA_REG;
			work.size = 8;
			work.data.str = new string("%r11");
			genMove(&work, src, "");
			src = &work;
		}
	}
	os << format("	subq %1%, %2%") % *src->data.str % *dst->data.str << endl;
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt)
{
	os << "	negq " << *tgt->data.str << endl;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getNull()
{
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_NULL;
	e->data.str = new string("$0");

	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getInt(int i)
{
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_CODE;
	e->data.str = new string((format("$%1%") % i).str());

	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getStackAddress(int offset, int size)
{
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_MEM;
	e->size = size;
	e->data.str = new string((format("-%1%(%%rbp)") % offset).str());

	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getStrAddress(int index)
{
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_MEM;
	e->data.str = new string((format("$.LC%1%") % index).str());

	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getArgument(int i, int size)
{
	BOOST_ASSERT(i>=0 && i <= 6);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	e->size = size;
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

unique_ptr<PlnGenEntity> PlnX86_64Generator::getSysArgument(int i)
{
	BOOST_ASSERT(i>=0 && i <= 6);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	e->size = 8;
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

unique_ptr<PlnGenEntity> PlnX86_64Generator::getWork(int i)
{
	BOOST_ASSERT(i>=0 && i <= 2);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_STRING;
	e->alloc_type = GA_REG;
	e->size = 8;
	switch (i) {
		case 0: e->data.str = new string("%rax");	break;
		case 1: e->data.str = new string("%rdi");	break;
		case 2: e->data.str = new string("%rsi");	break;
	}
	return e;
}