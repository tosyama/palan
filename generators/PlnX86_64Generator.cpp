/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "../PlnModel.h"
#include "../PlnConstants.h"
#include "PlnX86_64DataAllocator.h"
#include "PlnX86_64Generator.h"

using std::ostringstream;
using std::to_string;
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

		tbl[R11][0] = "%r11b"; tbl[R11][1] = "%r11w";
		tbl[R11][2] = "%r11d"; tbl[R11][3] = "%r11";
	}
	// 1->0, 2->1, 4->2, 8->3
	int i = (size & 7) ? size >> 1 : 3;
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

void PlnX86_64Generator::genJumpLabel(int id, string comment)
{
	os << ".L" << id << ":";
	if (comment != "")
		os << "		# " << comment;
	os << endl;
}

void PlnX86_64Generator::genJump(int id, string comment)
{
	os << "	jmp .L" << id;
	if (comment != "")
		os << "		# " << comment;
	os << endl;
}

void PlnX86_64Generator::genTrueJump(int id, int cmp_type, string comment)
{
	const char* jcmd;
	switch (cmp_type) {
		case CMP_EQ: jcmd = "je"; break;
		case CMP_NE: jcmd = "jne"; break;
		case CMP_L: jcmd = "jl"; break;
		case CMP_G: jcmd = "jg"; break;
		case CMP_LE: jcmd = "jle"; break;
		case CMP_GE: jcmd = "jge"; break;
		case CMP_B: jcmd = "jb"; break;
		case CMP_A: jcmd = "ja"; break;
		case CMP_BE: jcmd = "jbe"; break;
		case CMP_AE: jcmd = "jae"; break;
			break;

		defalt:
			BOOST_ASSERT(false);
	}
	os << "	" << jcmd << " .L" << id;
	if (comment != "")
		os << "		# " << comment;
	os << endl;
}

void PlnX86_64Generator::genFalseJump(int id, int cmp_type, string comment)
{
	const char* jcmd;
	switch (cmp_type) {
		case CMP_EQ: jcmd = "jne"; break;
		case CMP_NE: jcmd = "je"; break;
		case CMP_L: jcmd = "jge"; break;
		case CMP_G: jcmd = "jle"; break;
		case CMP_LE: jcmd = "jg"; break;
		case CMP_GE: jcmd = "jl"; break;
		case CMP_B: jcmd = "jae"; break;
		case CMP_A: jcmd = "jbe"; break;
		case CMP_BE: jcmd = "ja"; break;
		case CMP_AE: jcmd = "jb"; break;
			break;

		defalt:
			BOOST_ASSERT(false);
	}
	os << "	" << jcmd << " .L" << id;
	if (comment != "")
		os << "		# " << comment;
	os << endl;
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
		os << "	subq $" << size << ", %rsp" << endl;
	}
}

void PlnX86_64Generator::genFreeLocalVarArea(int size)
{
	if (size) {
		os << "	addq $" << size << ", %rsp" << endl;
	}
}

void PlnX86_64Generator::genSaveReg(int reg, PlnGenEntity* dst)
{
	os << "	movq " << r(reg) << ", " << oprnd(dst) << endl;
}

void PlnX86_64Generator::genLoadReg(int reg, PlnGenEntity* src)
{
	os << "	movq " << oprnd(src) << ", " << r(reg) << endl;
}


void PlnX86_64Generator::genCCall(string& cfuncname)
{
	os << "	xorq %rax, %rax" << endl;
	os << "	call " << cfuncname << endl;
}

void PlnX86_64Generator::genSysCall(int id, const string& comment)
{
	os << "	movq $" << id << ", %rax	# " <<  comment << endl;
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
	os << ".LC" << index << ":" << endl;
	string ostr = replace_all_copy(str,"\n","\\n");
	os << "	.string \"" << ostr << "\"" << endl;
}

void PlnX86_64Generator::moveMemToReg(const PlnGenEntity* mem, int reg)
{
	BOOST_ASSERT(mem->alloc_type == GA_MEM);
	string dst_safix = "q";
	string src_safix = "";

	const char* srcstr = oprnd(mem);
	const char* dststr = r(reg, 8);

	if (mem->data_type == DT_SINT) {
		switch (mem->size) {
			case 1: src_safix = "sb"; break;
			case 2: src_safix = "sw"; break;
			case 4: src_safix = "sl"; break;
		}
	} else { // unsigned
		switch (mem->size) {
			case 1: src_safix = "zb"; break;
			case 2: src_safix = "zw"; break;
			case 4:
					src_safix = "";
					dststr = r(reg, 4);
					dst_safix = "l";
					break;
		}
	}

	os << "	mov" << src_safix << dst_safix << " " << srcstr << ", " << dststr;
}

static bool needAbsCopy(const PlnGenEntity* immediate)
{
	BOOST_ASSERT(immediate->alloc_type == GA_CODE);
	const char* ints = immediate->data.str->c_str();
	if (ints[1] == '.') {	// simbol.
		return false;

	} else if (ints[1] == '-') {
		ints+=2;
		int len = strlen(ints);
		if (strlen(ints) > 10 || (len==10&&strcmp(ints, "2147483648") > 0))
			return true;

	} else {
		ints++;
		int len = strlen(ints);
		if (strlen(ints) > 10 || (len==10&&strcmp(ints, "4294967295") > 0))
			return true;
	}
	return false;
}

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	if (dst->alloc_type == src->alloc_type) {
		if (!strcmp(oprnd(dst), oprnd(src))) return;	// do nothing
	}

	if (dst->alloc_type == GA_NULL) return;

	string dst_safix;
	switch (dst->size) {
		case 1: dst_safix = "b"; break;
		case 2: dst_safix = "w"; break;
		case 4: dst_safix = "l"; break;
		case 8: dst_safix = "q"; break;
		default:
			BOOST_ASSERT(false);
	}

	// Reg -> Reg or immediate integer copy
	if (src->alloc_type == GA_REG && dst->alloc_type == GA_REG
			|| src->alloc_type == GA_CODE && !needAbsCopy(src)) {
		os << "	mov" << dst_safix << " " << oprnd(src) << ", " << oprnd(dst);
		if (comment != "") os << "	# " << comment;
		os << endl;
		return;
	}

	int regid = R11;	// work register.
	if (dst->alloc_type == GA_REG) {
		regid = dst->data.i;
	}

	// Load to registor.
	bool pre_process = false;
	if (src->alloc_type == GA_MEM) {
		moveMemToReg(src, regid);
		pre_process = true;
	} else if (src->alloc_type == GA_CODE) {
		os << "	movabsq " << oprnd(src) << ", " << r(regid, 8);
		pre_process = true;
	} else	// src->alloc_type == RA_REG
		regid = src->data.i;

	// Reg -> Memory
	if (dst->alloc_type == GA_MEM) {
		if (pre_process) os << endl;
		os << "	mov" << dst_safix << " " << r(regid, dst->size) << ", " << oprnd(dst);
	}

	if (comment != "") os << "	# " << comment;
	os << endl;
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* second, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);
	if (second->alloc_type == GA_CODE && *second->data.str == "$1") {
		os << "	incq " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	const char* add_str = oprnd(second);
	if (second->alloc_type == GA_MEM && second->size < 8) {
		moveMemToReg(second, R11);
		os << endl;
		add_str = r(R11,8);
	}
		
	os << "	addq " << add_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* second, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);
	if (second->alloc_type == GA_CODE && *second->data.str == "$1") {
		os << "	decq " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	const char* sub_str = oprnd(second);
	if (second->alloc_type == GA_MEM && second->size < 8) {
		moveMemToReg(second, R11);
		os << endl;
		sub_str = r(R11,8);
	}

	os << "	subq " << sub_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt, string comment)
{
	os << "	negq " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* second, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || second->alloc_type != GA_MEM);

	const char* mul_str = oprnd(second);
	if (second->alloc_type == GA_MEM && second->size < 8) {
		moveMemToReg(second, R11);
		os << endl;
		mul_str = r(R11,8);
	}
	os << "	imulq " << mul_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* second, string comment)
{
	BOOST_ASSERT(tgt->alloc_type == GA_REG && tgt->data.i == RAX);

	const char* div_str = oprnd(second);
	if (second->alloc_type == GA_CODE) { 
		div_str = r(R11, 8);
		if (needAbsCopy(second)) {
			os << "	movabsq " << oprnd(second) << ", " << div_str << endl;
		} else {
			os << "	movq " << oprnd(second) << ", " << div_str << endl;
		}
	}

	if (tgt->data_type == DT_UINT && second->data_type == DT_UINT) {
		os << "	movq $0, %rdx" << endl;
		os << "	divq " << div_str << "	# " << comment << endl;
	} else {
		os << "	cqto"	<< endl;
		os << "	idivq " << div_str << "	# " << comment << endl;
	}
}

int PlnX86_64Generator::genCmp(PlnGenEntity* first, PlnGenEntity* second, int cmp_type, string comment)
{
	if ((second->alloc_type != GA_CODE && first->alloc_type == GA_CODE) 
			|| (second->alloc_type == GA_MEM && first->alloc_type != GA_MEM)
			|| (second->alloc_type == GA_MEM && first->alloc_type == GA_MEM
				&& second->size > first->size)) {
		// swap
		auto tmp = second;
		second = first;
		first = tmp;
		switch (cmp_type) {
			case CMP_L: cmp_type = CMP_G; break;
			case CMP_G: cmp_type = CMP_L; break;
			case CMP_LE: cmp_type = CMP_GE; break;
			case CMP_GE: cmp_type = CMP_LE; break;
		}
	}

	if (first->data_type == DT_UINT && second->data_type == DT_UINT) {
		switch (cmp_type) {
			case CMP_L: cmp_type = CMP_B; break;
			case CMP_G: cmp_type = CMP_A; break;
			case CMP_LE: cmp_type = CMP_BE; break;
			case CMP_GE: cmp_type = CMP_AE; break;
		}
	}
	BOOST_ASSERT(first->alloc_type != GA_CODE);

	const char* s_str;
	if (second->alloc_type == GA_REG) {
		s_str = r(second->data.i, first->size);
	} else if (second->alloc_type == GA_MEM) {
		moveMemToReg(second, R11);
		os << endl;
		s_str = r(R11, first->size);
	} else
		s_str = oprnd(second);

	const char* safix = "";
	if (first->alloc_type == GA_MEM) {
		switch (first->size) {
			case 1: safix = "b"; break;
			case 2: safix = "w"; break;
			case 4: safix = "l"; break;
			case 8: safix = "q"; break;
			default:
				BOOST_ASSERT(false);
		}
	}

	os << "	cmp" << safix << " " << s_str << ", " << oprnd(first) ;
	os << "	# " << comment << endl;

	return cmp_type;
}

int PlnX86_64Generator::genMoveCmpFlag(PlnGenEntity* tgt, int cmp_type, string comment)
{
	BOOST_ASSERT(tgt->alloc_type == GA_REG);
	BOOST_ASSERT(tgt->size == 8);

	const char* byte_reg = r(tgt->data.i, 1);
	const char* setcmd =
		cmp_type == CMP_EQ ? "sete":
		cmp_type == CMP_NE ? "setne":
		cmp_type == CMP_L ? "setl":
		cmp_type == CMP_G ? "setg":
		cmp_type == CMP_LE ? "setle":
		cmp_type == CMP_GE ? "setge":
		cmp_type == CMP_B ? "setb":
		cmp_type == CMP_A ? "seta":
		cmp_type == CMP_BE ? "setbe":
		cmp_type == CMP_AE ? "setae":
		NULL;

	BOOST_ASSERT(setcmd);
	
	os << "	" << setcmd << " " << byte_reg;
	if (comment != "")
		os << "	# " << comment;
	
	os << endl;
	os << "	movzbq " << byte_reg << ", " << oprnd(tgt) << endl;
}

void PlnX86_64Generator::genNullClear(vector<unique_ptr<PlnGenEntity>> &refs)
{
	if (refs.size() == 1)
		os << "	movq $0, " << oprnd(refs[0].get()) << endl;
	else if (refs.size() >= 2) {
		os << "	xorq %rax, %rax" << endl;
		for (auto& r: refs)
			os << "	movq %rax, " << oprnd(r.get()) << endl;
	}
}

void PlnX86_64Generator::genMemAlloc(PlnGenEntity* ref, int al_size, string& comment)
{
	os << "	movq $" << al_size << ", %rdi"	<< endl;
	os << "	call malloc" << endl;
	os << "	movq %rax, " << oprnd(ref);
	
	if (comment != "") os << "	# alloc " << comment;
	os << endl;
}

void PlnX86_64Generator::genMemCopy(int cp_size, string& comment)
{
	int cpy_count;
	const char* safix = "";
	if (cp_size % 8 == 0) {
		cpy_count = cp_size/8;
		safix = "q";
	} else if (cp_size % 4 == 0) {
		cpy_count = cp_size/4;
		safix = "l";
	} else if (cp_size % 2 == 0) {
		cpy_count = cp_size/2;
		safix = "w";
	} else {
		cpy_count = cp_size;
		safix = "b";
	}

	os << "	cld" << endl;
	os << "	movq $" << cpy_count << ", %rcx"	<< endl;
	os << "	rep movs" << safix << "	# " << comment << endl;
}

void PlnX86_64Generator::genMemFree(PlnGenEntity* ref, string& comment, bool doNull)
{
	os << "	movq " << oprnd(ref) << ", %rdi"	<< endl;
	os << "	call free";

	if (doNull) {
		os << endl << "	movq $0, " << oprnd(ref);
	}

	if (comment != "") os << "	# free " << comment;
	os << endl;
}

static string* getAdressingStr(int displacement, int base_id, int index_id, int scale)
{
	string *str = new string(r(base_id));
	string disp_s = displacement ? to_string(displacement) : "";
	string ind_s = string(r(index_id)) + "," + to_string(scale);
	*str = disp_s + "(" + *str + "," + ind_s + ")";

	return str;
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getEntity(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->data_type != DT_UNKNOWN);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	if (dp->type == DP_STK_BP) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.str = new string(to_string(dp->data.stack.offset) + "(%rbp)");

	} else if (dp->type == DP_STK_SP) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.str = new string(to_string(dp->data.stack.offset) + "(%rsp)");

	} else if (dp->type == DP_REG) {
		e->type = GE_INT;
		e->alloc_type = GA_REG;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.i = dp->data.reg.id;

	} else if (dp->type == DP_INDRCT_OBJ) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.str = getAdressingStr(
				dp->data.indirect.displacement,
				dp->data.indirect.base_id,
				dp->data.indirect.index_id,
				dp->size
			);

	} else if (dp->type == DP_LIT_INT) {
		e->type = GE_STRING;
		e->alloc_type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->data.str = new string(string("$") + to_string(dp->data.intValue));

	} else if (dp->type == DP_RO_DATA) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = 8;
		e->data_type = DT_OBJECT_REF;
		e->data.str = new string(string("$.LC") + to_string(dp->data.index));

	} else
		BOOST_ASSERT(false);

	return e;
}

