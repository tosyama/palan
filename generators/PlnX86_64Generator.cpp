/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/format.hpp>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "../PlnModel.h"
#include "PlnX86_64DataAllocator.h"
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

static const char* r(int rt, int size=8)
{
	static const char* tbl[16][4];
	static bool init = false;
	if (!init) {
		init = true;
		tbl[RAX][0] = "%al"; tbl[RAX][1] = "%ax";
		tbl[RAX][2] = "%eax"; tbl[RAX][3] = "%rax";
		
		tbl[RBX][0] = "%bl"; tbl[RBX][1] = "%bx";
		tbl[RBX][2] = "%ebx"; tbl[RBX][3] = "%rbx";

		tbl[RCX][0] = "%cl"; tbl[RCX][1] = "%cx";
		tbl[RCX][2] = "%ecx"; tbl[RCX][3] = "%rcx";

		tbl[RDX][0] = "%dl"; tbl[RDX][1] = "%dx";
		tbl[RDX][2] = "%edx"; tbl[RDX][3] = "%rdx";

		tbl[RDI][0] = "%dil"; tbl[RDI][1] = "%di";
		tbl[RDI][2] = "%edi"; tbl[RDI][3] = "%rdi";

		tbl[RSI][0] = "%sil"; tbl[RSI][1] = "%si";
		tbl[RSI][2] = "%esi"; tbl[RSI][3] = "%rsi";

		tbl[RBP][0] = "%bpl"; tbl[RBP][1] = "%bp";
		tbl[RBP][2] = "%ebp"; tbl[RBP][3] = "%rbp";

		tbl[RSP][0] = "%spl"; tbl[RSP][1] = "%sp";
		tbl[RSP][2] = "%esp"; tbl[RSP][3] = "%rsp";

		tbl[R8][0] = "%r8b"; tbl[R8][1] = "%r8w";
		tbl[R8][2] = "%r8d"; tbl[R8][3] = "%r8";
		
		tbl[R9][0] = "%r9b"; tbl[R9][1] = "%r9w";
		tbl[R9][2] = "%r9d"; tbl[R9][3] = "%r9";

		tbl[R10][0] = "%r10b"; tbl[R10][1] = "%r10w";
		tbl[R10][2] = "%r10d"; tbl[R10][3] = "%r10";
	}
	// 1->0, 2->1, 4->2, 8->3
	int i = (size & 3) ? size >> 1 : 3;
	return tbl[rt][i];
}

static const char* oprnd(const PlnGenEntity *e)
{
	if (e->type == GE_STRING) {
		return e->data.str->c_str();
	} else if (e->type == GE_INT) {
		if (e->alloc_type == GA_REG)
			return r(e->data.i, e->size);
	}
	BOOST_ASSERT(false);
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
	if (entryname == "")
		os << "_start" << endl;
	else
		os << entryname << endl;
}

void PlnX86_64Generator::genLabel(const string& label)
{
	if (label == "") os << "_start:" << endl;
	else os << label << ":" << endl;
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

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	string dst_safix = "q";
	string src_safix = "";

	const char* srcstr = oprnd(src);
	if (dst->alloc_type == GA_MEM) {
		switch (dst->size) {
			case 1: dst_safix = "b"; break;
			case 2: dst_safix = "w"; break;
			case 4: dst_safix = "l"; break;
		}
		if (src->alloc_type == GA_REG && dst->size<=4)
			srcstr = r(src->data.i, dst->size);
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
		if (!strcmp(oprnd(dst), oprnd(src))) return;	// do nothing
	}

	if (dst->alloc_type == GA_NULL) return;

	if (dst->alloc_type != GA_MEM || src->alloc_type != GA_MEM) {
		os << "	mov" << src_safix << dst_safix << " " << srcstr << ", " << oprnd(dst);
	} else {
		os << "	mov" << src_safix << dst_safix << " " << srcstr << ", %rax" << endl;
		os << "	mov" << dst_safix << " %rax, " << oprnd(dst);
	}
	if (comment != "") os << "	# " << comment;
	os << endl;
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* second)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);
	if (second->alloc_type == GA_CODE && *second->data.str == "$1") {
		os << "	incq " << oprnd(tgt) << endl;
		return;
	}

	// TODO: templary imprement. need to refacter.
	PlnGenEntity work;
	if (second->alloc_type == GA_MEM) {
		if (second->size < 8) {
			work.type = GE_STRING;
			work.alloc_type = GA_REG;
			work.size = 8;
			work.data.str = new string("%r11");
			genMove(&work, second, "");
			second = &work;
		}
	}
		
	os << "	addq " << oprnd(second) << ", " << oprnd(tgt) << endl;
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* second)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);
	if (second->alloc_type == GA_CODE && *second->data.str == "$1") {
		os << "	decq " << oprnd(tgt) << endl;
		return;
	}
	// TODO: templary imprement. need to refacter.
	PlnGenEntity work;
	if (second->alloc_type == GA_MEM) { 
		if (second->size < 8) {
			work.type = GE_STRING;
			work.alloc_type = GA_REG;
			work.size = 8;
			work.data.str = new string("%r11");
			genMove(&work, second, "");
			second = &work;
		}
	}
	os << format("	subq %1%, %2%") % oprnd(second) % oprnd(tgt) << endl;
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt)
{
	os << "	negq " << oprnd(tgt) << endl;
}

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* second)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);
	// TODO: templary imprement. need to refacter.
	PlnGenEntity work;
	if (second->alloc_type == GA_MEM) { 
		if (second->size < 8) {
			work.type = GE_STRING;
			work.alloc_type = GA_REG;
			work.size = 8;
			work.data.str = new string("%r11");
			genMove(&work, second, "");
			second = &work;
		}
	}
	os << format("	imulq %1%, %2%") % oprnd(second) % oprnd(tgt) << endl;
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* second)
{
	BOOST_ASSERT(tgt->alloc_type == GA_REG && tgt->data.i == RAX);
	PlnGenEntity work;
	if (second->alloc_type != GA_REG) { 
		work.type = GE_STRING;
		work.alloc_type = GA_REG;
		work.size = 8;
		work.data.str = new string("%r11");
		genMove(&work, second, "");
		second = &work;
	}
	os << "	cqto"	<< endl;
	os << "	idivq " << oprnd(second) << endl;
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
	e->type = GE_INT;
	e->alloc_type = GA_REG;
	e->size = size;
	switch (i) {
		case 0: e->data.i = RAX;	break;
		case 1: e->data.i = RDI;	break;
		case 2: e->data.i = RSI;	break;
		case 3: e->data.i = RDX;	break;
		case 4: e->data.i = RCX;	break;
		case 5: e->data.i = R8;	break;
		case 6: e->data.i = R9;	break;
	}
	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getSysArgument(int i)
{
	BOOST_ASSERT(i>=0 && i <= 6);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	e->type = GE_INT;
	e->alloc_type = GA_REG;
	e->size = 8;
	switch (i) {
		case 0: e->data.i = RAX;	break;
		case 1: e->data.i = RDI;	break;
		case 2: e->data.i = RSI;	break;
		case 3: e->data.i = RDX;	break;
		case 4: e->data.i = R10;	break;
		case 5: e->data.i = R8;	break;
		case 6: e->data.i = R9;	break;
	}
	return e;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getPushEntity(PlnDataPlace* dp)
{
	if (dp->save_place) {
		return getEntity(dp->save_place);
	} else
		return getEntity(dp);
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getPopEntity(PlnDataPlace* dp)
{
	auto e = getEntity(dp);
	if (dp->save_place) {
		auto se = getEntity(dp->save_place);
		string cmt = string("load ") + *dp->comment + " from " + *dp->save_place->comment;
		genMove(e.get(), se.get(), cmt);
	}
	return getEntity(dp);
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getEntity(PlnDataPlace* dp)
{
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	if (dp->type == DP_STK_BP) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data.str = new string((format("%1%(%%rbp)") % dp->data.stack.offset).str());
	} else if (dp->type == DP_STK_SP) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data.str = new string((format("%1%(%%rsp)") % dp->data.stack.offset).str());
	} else if (dp->type == DP_REG) {
		e->type = GE_INT;
		e->alloc_type = GA_REG;
		e->size = 8;
		e->data.i = dp->data.reg.id;
	} else if (dp->type == DP_LIT_INT)
		return getInt(dp->data.intValue);
	else if (dp->type == DP_RO_DATA)
		return getStrAddress(dp->data.index);
	else
		BOOST_ASSERT(false);
	return e;
}

