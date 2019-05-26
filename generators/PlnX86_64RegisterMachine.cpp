/// x86-64 (Linux) register machine class definition.
///
/// @file	PlnX86_64RegisterMachine.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "../PlnModel.h"
#include "PlnX86_64DataAllocator.h"
#include "PlnX86_64Generator.h"
#include "PlnX86_64RegisterMachine.h"

static const char* r(int rt, int size)
{
	BOOST_ASSERT(rt < REG_NUM);

	static const char* tbl[REG_NUM][4];
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

		tbl[XMM0][0] = "%xmm0"; tbl[XMM0][1] = "%xmm0";
		tbl[XMM0][2] = "%xmm0"; tbl[XMM0][3] = "%xmm0";

		tbl[XMM1][0] = "%xmm1"; tbl[XMM1][1] = "%xmm1";
		tbl[XMM1][2] = "%xmm1"; tbl[XMM1][3] = "%xmm1";

		tbl[XMM2][0] = "%xmm2"; tbl[XMM2][1] = "%xmm2";
		tbl[XMM2][2] = "%xmm2"; tbl[XMM2][3] = "%xmm2";

		tbl[XMM3][0] = "%xmm3"; tbl[XMM3][1] = "%xmm3";
		tbl[XMM3][2] = "%xmm3"; tbl[XMM3][3] = "%xmm3";

		tbl[XMM4][0] = "%xmm4"; tbl[XMM4][1] = "%xmm4";
		tbl[XMM4][2] = "%xmm4"; tbl[XMM4][3] = "%xmm4";

		tbl[XMM5][0] = "%xmm5"; tbl[XMM5][1] = "%xmm5";
		tbl[XMM5][2] = "%xmm5"; tbl[XMM5][3] = "%xmm5";

		tbl[XMM6][0] = "%xmm6"; tbl[XMM6][1] = "%xmm6";
		tbl[XMM6][2] = "%xmm6"; tbl[XMM6][3] = "%xmm6";

		tbl[XMM7][0] = "%xmm7"; tbl[XMM7][1] = "%xmm7";
		tbl[XMM7][2] = "%xmm7"; tbl[XMM7][3] = "%xmm7";

		tbl[XMM8][0] = "%xmm8"; tbl[XMM8][1] = "%xmm8";
		tbl[XMM8][2] = "%xmm8"; tbl[XMM8][3] = "%xmm8";

		tbl[XMM9][0] = "%xmm9"; tbl[XMM9][1] = "%xmm9";
		tbl[XMM9][2] = "%xmm9"; tbl[XMM9][3] = "%xmm9";

		tbl[XMM10][0] = "%xmm10"; tbl[XMM10][1] = "%xmm10";
		tbl[XMM10][2] = "%xmm10"; tbl[XMM10][3] = "%xmm10";

		tbl[XMM11][0] = "%xmm11"; tbl[XMM11][1] = "%xmm11";
		tbl[XMM11][2] = "%xmm11"; tbl[XMM11][3] = "%xmm11";

		tbl[XMM12][0] = "%xmm12"; tbl[XMM12][1] = "%xmm12";
		tbl[XMM12][2] = "%xmm12"; tbl[XMM12][3] = "%xmm12";

		tbl[XMM13][0] = "%xmm13"; tbl[XMM13][1] = "%xmm13";
		tbl[XMM13][2] = "%xmm13"; tbl[XMM13][3] = "%xmm13";

		tbl[XMM14][0] = "%xmm14"; tbl[XMM14][1] = "%xmm14";
		tbl[XMM14][2] = "%xmm14"; tbl[XMM14][3] = "%xmm14";

		tbl[XMM15][0] = "%xmm15"; tbl[XMM15][1] = "%xmm15";
		tbl[XMM15][2] = "%xmm15"; tbl[XMM15][3] = "%xmm15";

		tbl[RIP][0] = "%rip"; tbl[RIP][1] = "%rip";
		tbl[RIP][2] = "%rip"; tbl[RIP][3] = "%rip";
	}

	if (rt < XMM0) {
		// 1->0, 2->1, 4->2, 8->3
		int i = (size & 7) ? size >> 1 : 3;
		return tbl[rt][i];

	} else {
		int i;
		switch (size) {
			case 4: case 8:
				i = 0; break;
			defalt:
				BOOST_ASSERT(false);
		}
		return tbl[rt][i];
	}
}

const char* PlnRegOperand::str(char* buf)
{
	return r(regid, size);
}

const char* PlnImmOperand::str(char* buf)
{
	sprintf(buf, "$%ld", value);
	return buf;
}

const char* PlnAdrsModeOperand::str(char* buf)
{
	char *buf2 = buf;
	if (displacement != 0)
		buf2 += sprintf(buf, "%d", displacement);
	if (index_regid == -1) sprintf(buf2, "(%s)", r(base_regid,8));
	else sprintf(buf2, "(%s,%s,%d)", r(base_regid,8), r(index_regid,8), scale);

	return buf;
}

const char* PlnLabelOperand::str(char* buf)
{
	if (id == -1) return label.c_str();
	sprintf(buf, "%s%d", label.c_str(), id);
	return buf;
}

const char* PlnLabelAdrsModeOperand::str(char* buf)
{
	sprintf(buf, "%s(%s)", label.c_str(), r(base_regid,8));
	return buf;
}

static vector<const char*> mnes;

PlnX86_64RegisterMachine::PlnX86_64RegisterMachine() : has_call(false), may_exist_rounding(0)
{}

static void initMnes()
{
	mnes.reserve(MNE_SIZE);
	mnes[COMMENT] = "#";
	mnes[LABEL] = ":";

	mnes[CALL] = "call";
	mnes[SYSCALL] = "syscall";

	mnes[JMP] = "jmp";
	mnes[JNE] = "jne";
	mnes[JE] = "je";
	mnes[JGE] = "jge";
	mnes[JLE] = "jle";
	mnes[JG] = "jg";
	mnes[JL] = "jl";
	mnes[JAE] = "jae";
	mnes[JBE] = "jbe";
	mnes[JA] = "ja";
	mnes[JB] = "jb";

	mnes[LEA] = "lea";
	mnes[LEAVE] = "leave";
	mnes[RET] = "ret";

	mnes[POPQ] = "popq";
	mnes[PUSHQ] = "pushq";

	mnes[MOVABSQ] = "movabsq";
	mnes[MOVB] = "movb";
	mnes[MOVW] = "movw";
	mnes[MOVL] = "movl";
	mnes[MOVQ] = "movq";
	mnes[MOVSBQ] = "movsbq";
	mnes[MOVSWQ] = "movswq";
	mnes[MOVSLQ] = "movslq";
	mnes[MOVZBQ] = "movzbq";
	mnes[MOVZWQ] = "movzwq";
	mnes[MOVSS] = "movss";
	mnes[MOVSD] = "movsd";

	mnes[ADDSD] = "addsd";
	mnes[INCQ] = "incq";
	mnes[ADDQ] = "addq";

	mnes[SUBSD] = "subsd";
	mnes[DECQ] = "decq";
	mnes[SUBQ] = "subq";
	mnes[NEGQ] = "negq";

	mnes[MULSD] = "mulsd";
	mnes[IMULQ] = "imulq";
	mnes[DIVSD] = "divsd";
	mnes[DIVQ] = "divq";
	mnes[CQTO] = "cqto";
	mnes[IDIVQ] = "idivq";

	mnes[XORQ] = "xorq";
	mnes[XORPD] = "xorpd";

	mnes[CVTTSD2SI] = "cvttsd2si";
	mnes[CVTTSS2SI] = "cvttss2si";
	mnes[CVTSD2SS] = "cvtsd2ss";
	mnes[CVTSI2SS] = "cvtsi2ss";
	mnes[CVTSI2SD] = "cvtsi2sd";
	mnes[CVTSS2SD] = "cvtss2sd";

	mnes[UCOMISS] = "ucomiss";
	mnes[UCOMISD] = "ucomisd";

	mnes[CMP] = "cmp";
	mnes[CMPB] = "cmpb";
	mnes[CMPW] = "cmpw";
	mnes[CMPL] = "cmpl";
	mnes[CMPQ] = "cmpq";

	mnes[SETE] = "sete";
	mnes[SETNE] = "setne";
	mnes[SETL] = "setl";
	mnes[SETG] = "setg";
	mnes[SETLE] = "setle";
	mnes[SETGE] = "setge";
	mnes[SETB] = "setb";
	mnes[SETA] = "seta";
	mnes[SETBE] = "setbe";
	mnes[SETAE] = "setae";

	mnes[REP_MOVSQ] = "rep movsq";
	mnes[REP_MOVSL] = "rep movsl";
	mnes[REP_MOVSW] = "rep movsw";
	mnes[REP_MOVSB] = "rep movsb";
	mnes[CLD] = "cld";
}

void PlnX86_64RegisterMachine::push(PlnX86_64Mnemonic mne, PlnOperandInfo *src, PlnOperandInfo* dst, string comment)
{
	opecodes.push_back({mne, src, dst, comment});
	// collect optimise information.
	if (mne == CALL) has_call = true;
	else if (mne == MOVABSQ && may_exist_rounding==0 && int64_of(src)==4602678819172646912) {
		may_exist_rounding = 1;	
	} else if (mne == CVTTSD2SI && may_exist_rounding==1) {
		may_exist_rounding = 2;	
	}
}

void PlnX86_64RegisterMachine::addComment(string comment)
{
	// BOOST_ASSERT(opecodes.back().comment == "");
	opecodes.back().comment = comment;
}

void PlnX86_64RegisterMachine::removeStackArea()
{
	auto opec = opecodes.begin();
	while (opec != opecodes.end()) {
		if (opec->mne == SUBQ && opec->dst->type == OP_REG
				&& regid_of(opec->dst) == RSP) {
			opec = opecodes.erase(opec);
		} else {
			if (opec->mne == LEAVE) {
				opec->mne = POPQ;
				opec->src = new PlnRegOperand(RBP,8);
			}
			opec++;
		}
	}
}

static ostream& operator<<(ostream& out, const PlnOpeCode& oc)
{
	char buf1[256], buf2[256];

	if (oc.mne == COMMENT) {
		if (oc.comment != "") out << "# " << oc.comment;
		return out;
	}

	if (oc.mne == LABEL) {
		BOOST_ASSERT(oc.src->type == OP_LBL);
		out << oc.src->str(buf1) << ":";
		if (oc.comment != "") out << "	# " << oc.comment;
		return out;
	}

	out << "	" << mnes[oc.mne];
	if (oc.src) out << " " << oc.src->str(buf1);
	if (oc.dst) out << ", " << oc.dst->str(buf2);
	if (oc.comment != "") out << "	# " << oc.comment;

	return out;
}

void PlnX86_64RegisterMachine::popOpecodes(ostream& os)
{
	if (!mnes.size())
		initMnes();

	// Optimize
	if (!has_call) removeStackArea();

	for (PlnOpeCode& oc: opecodes) {
		os << oc << "\n";
		delete oc.src;
		delete oc.dst;
	}
	os.flush();
	opecodes.clear();

	has_call = false;
	may_exist_rounding = 0;
}
