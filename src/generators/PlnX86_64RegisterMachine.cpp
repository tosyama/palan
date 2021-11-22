/// x86-64 (Linux) register machine class definition.
///
/// @file	PlnX86_64RegisterMachine.cpp
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

#include <vector>
#include <algorithm>
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
#include "PlnX86_64RegisterMachineImp.h"
#include "PlnX86_64RegisterSave.h"

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

		tbl[R12][0] = "%r12b"; tbl[R12][1] = "%r12w";
		tbl[R12][2] = "%r12d"; tbl[R12][3] = "%r12";

		tbl[R13][0] = "%r13b"; tbl[R13][1] = "%r13w";
		tbl[R13][2] = "%r13d"; tbl[R13][3] = "%r13";

		tbl[R14][0] = "%r14b"; tbl[R14][1] = "%r14w";
		tbl[R14][2] = "%r14d"; tbl[R14][3] = "%r14";

		tbl[R15][0] = "%r15b"; tbl[R15][1] = "%r15w";
		tbl[R15][2] = "%r15d"; tbl[R15][3] = "%r15";

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
			default:
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

PlnX86_64RegisterMachine::PlnX86_64RegisterMachine()
	:  imp(new PlnX86_64RegisterMachineImp())
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
	mnes[ADDSS] = "addss";
	mnes[INCQ] = "incq";
	mnes[ADDQ] = "addq";

	mnes[SUBSD] = "subsd";
	mnes[SUBSS] = "subss";
	mnes[DECQ] = "decq";
	mnes[SUBQ] = "subq";
	mnes[NEGQ] = "negq";

	mnes[MULSD] = "mulsd";
	mnes[MULSS] = "mulss";
	mnes[IMULQ] = "imulq";
	mnes[DIVSD] = "divsd";
	mnes[DIVSS] = "divss";
	mnes[DIVQ] = "divq";
	mnes[CQTO] = "cqto";
	mnes[IDIVQ] = "idivq";
	mnes[CLTQ] = "cltq";

	mnes[XORQ] = "xorq";
	mnes[XORPD] = "xorpd";
	mnes[XORPS] = "xorps";

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

	mnes[ANDQ] = "andq";
	mnes[SALQ] = "salq";
	mnes[SARQ] = "sarq";
	mnes[SHRQ] = "shrq";
}

void PlnX86_64RegisterMachine::push(PlnX86_64Mnemonic mne, PlnOperandInfo *src, PlnOperandInfo* dst, string comment)
{
	imp->opecodes.push_back({mne, src, dst, comment});

	// collect optimise information.
	if (mne == CALL) imp->has_call = true;
	else if (mne == SYSCALL) imp->has_call = true;
	else if (mne == RET) imp->ret_num++;

	// Don't use PUSH/POP except RBP.
	BOOST_ASSERT(!((mne == PUSHQ || mne == POPQ) && regid_of(src) != RBP));
}

void PlnX86_64RegisterMachine::reserve(int num)
{
	for (int i=0; i<num; i++)
		imp->opecodes.push_back({MNE_NONE, NULL, NULL, ""});
}

void PlnX86_64RegisterMachine::addComment(const string& comment)
{
	imp->opecodes.back().comment = comment;
}

void PlnX86_64RegisterMachine::memoRequestedStackSize(int size)
{
	imp->requested_stack_size = size;
}

static ostream& operator<<(ostream& out, const PlnOpeCode& oc)
{
	char buf[256];

	if (oc.mne == COMMENT) {
		if (oc.comment != "") out << "# " << oc.comment;
		return out;
	}

	if (oc.mne == LABEL) {
		BOOST_ASSERT(oc.src->type == OP_LBL);
		out << oc.src->str(buf) << ":";
		if (oc.comment != "") out << "	# " << oc.comment;
		return out;
	}

	out << "	" << mnes[oc.mne];
	if (oc.src) out << " " << oc.src->str(buf);
	if (oc.dst) out << ", " << oc.dst->str(buf);
	if (oc.comment != "") out << "	# " << oc.comment;

	return out;
}

// Optimazations
static void removeStackArea(vector<PlnOpeCode> &opecodes);
static void removeOmittableMoveToReg(vector<PlnOpeCode> &opecodes);
static void asmOptimize(vector<PlnOpeCode> &opecodes);

void PlnX86_64RegisterMachine::popOpecodes(ostream& os)
{
	if (!mnes.size())
		initMnes();

	// Optimize
	removeOmittableMoveToReg(imp->opecodes);

	// Add registor save  // ret_num == 0: top level
	if (imp->ret_num == 1) addRegSave(imp->opecodes, imp->requested_stack_size);
	else if (imp->ret_num >= 2) addRegSaveWithCFAnalysis(imp->opecodes, imp->requested_stack_size);

	// Note: It may change RBP->RSP.
	// 		 So this should be execute after removeOmittableMoveToReg().
	if (!imp->has_call)
		removeStackArea(imp->opecodes);

	asmOptimize(imp->opecodes);

	os << ".balign 16\n";
	BOOST_ASSERT(imp->opecodes.front().mne == LABEL);
	PlnX86_64Mnemonic pre_mne = MNE_SIZE;
	for (PlnOpeCode& oc: imp->opecodes) {
		if (oc.mne == LABEL) {
			if (pre_mne == RET || pre_mne == JMP) {
				os << ".balign 2\n";
			}
		}
		if (oc.mne != MNE_NONE)
			os << oc << "\n";
		delete oc.src;
		delete oc.dst;
		if (!(oc.mne == MNE_NONE || oc.mne == COMMENT))
			pre_mne = oc.mne;
	}
	os.flush();
	// reset internal information.
	imp->opecodes.clear();
	imp->has_call = false;
	imp->ret_num = 0;
	imp->requested_stack_size = 0;
}

static void replaceRbp2Rsp(PlnOperandInfo* ope) {
	if (ope) {
		if (ope->type == OP_ADRS) {
			auto addr_ope = static_cast<PlnAdrsModeOperand*>(ope);
			if (addr_ope->base_regid == RBP) {
				addr_ope->base_regid = RSP;
				if (addr_ope->displacement > 0) {
					// adjust affect of remove push rbp
					addr_ope->displacement -= 8;
				}
			}
			BOOST_ASSERT(addr_ope->index_regid != RBP);
		}
		if (ope->type == OP_LBLADRS) {
			BOOST_ASSERT(false);
			// auto lbladdr_ope = static_cast<PlnLabelAdrsModeOperand*>(ope);
			// BOOST_ASSERT(lbladdr_ope->base_regid != RBP);
		}
	}
}

void removeStackArea(vector<PlnOpeCode> &opecodes)
{
	for (auto &opec: opecodes) {
		if ((opec.mne == SUBQ || opec.mne == ADDQ) && opec.dst->type == OP_REG && regid_of(opec.dst) == RSP)
			opec.mne = MNE_NONE;
		else if (opec.mne == MOVQ && opec.src->type == OP_REG && regid_of(opec.src) == RSP
				&& opec.dst->type == OP_REG && regid_of(opec.dst) == RBP)
			opec.mne = MNE_NONE;
		else if (opec.mne == PUSHQ && opec.src->type == OP_REG && regid_of(opec.src) == RBP)
			opec.mne = MNE_NONE;
		else if (opec.mne == POPQ && opec.src->type == OP_REG && regid_of(opec.src) == RBP)
			opec.mne = MNE_NONE;
		else {
			replaceRbp2Rsp(opec.src);
			replaceRbp2Rsp(opec.dst);
		}
	}
}

// Remove omiitalbe move.
enum RegState {
	RS_UNKONWN,
	RS_IMM_INT,
	RS_STACK_VAR,
	RS_REG_VAR
};

class Reg
{
public:
	RegState state;
	struct {
		int displacement;
		int regid;
	} src;
	int size;
};

static void breakRegsInfo(Reg *regs)
{
	// Same as PlnX86_64DataAllocator.cpp
	static const int DSTRY_TBL[] = { RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11};
	for (int id: DSTRY_TBL)
		regs[id].state = RS_UNKONWN;
	for (int id = XMM0; id<REG_NUM; id++)
		regs[id].state = RS_UNKONWN;

	for (int i=0; i<XMM0; i++) {
		if (regs[i].state == RS_REG_VAR) {
			auto &reg = regs[i];
			if (reg.src.regid >= XMM0) {
				reg.state = RS_UNKONWN;
				continue;
			}
			for (int id: DSTRY_TBL) {
				if (reg.src.regid == id) {
					reg.state = RS_UNKONWN;
					break;
				}
			}
		}
	}
}

void removeOmittableMoveToReg(vector<PlnOpeCode> &opecodes)
{
	Reg regs[REG_NUM];
	for (auto& r: regs)
		r.state = RS_UNKONWN;
	
	for (auto &opec: opecodes) {
		if (opec.mne == MNE_NONE)
			continue;

		if (opec.dst) {	// 2 params
			if (opec.dst->type == OP_REG) {
				int dst_regid = regid_of(opec.dst);
				auto &reg = regs[dst_regid];
				if (opec.mne == MOVQ) {
					if (opec.src->type == OP_ADRS) {
						auto src = static_cast<PlnAdrsModeOperand*>(opec.src);
						if (src->base_regid == RBP && src->index_regid==-1) {
							// Load local var to the register.
							if (reg.state == RS_STACK_VAR && reg.size == 8
									&& reg.src.displacement == src->displacement) {
								opec.mne = MNE_NONE;
								continue;
								
							} else {
								reg.state = RS_STACK_VAR;
								reg.size = 8;
								reg.src.displacement = src->displacement;
							}
						} else {
							reg.state = RS_UNKONWN;
						}
					} else if (opec.src->type == OP_REG) {
						auto src = static_cast<PlnRegOperand*>(opec.src);
						if (reg.state == RS_REG_VAR && reg.src.regid == src->regid && reg.size == 8) {
							opec.mne = MNE_NONE;
							continue;
						}
						reg.state = RS_REG_VAR;
						reg.size = 8;
						reg.src.regid = regid_of(opec.src);
					} else {
						reg.state = RS_UNKONWN;
					}
				} else {
					reg.state = RS_UNKONWN;
				}

				for (int i=0; i<REG_NUM; i++) {
					auto &r = regs[i];
					if (r.state == RS_REG_VAR && r.src.regid == dst_regid && i != dst_regid) {
						r.state = RS_UNKONWN;
					}
				}
				continue;
				// end of dst == reg
			} else if (opec.dst->type == OP_ADRS) {
				auto dst = static_cast<PlnAdrsModeOperand*>(opec.dst);
				if (dst->base_regid == RBP && dst->index_regid==-1) {
					// local var update
					for (auto& r: regs) {
						if (r.state == RS_STACK_VAR && r.src.displacement == dst->displacement)
							r.state = RS_UNKONWN;
					}
				}
			}

		} else if (opec.src) {	// 1 param
			if (opec.src->type == OP_REG) {
				switch (opec.mne) {
					case INCQ: case DECQ: case NEGQ:
					case SETE: case SETNE:
					case SETL: case SETG: case SETLE: case SETGE:
					case SETB: case SETA: case SETBE: case SETAE:
						regs[regid_of(opec.src)].state = RS_UNKONWN;
						break;
					case IMULQ: case DIVQ: case CQTO: case IDIVQ:
						regs[RAX].state = RS_UNKONWN;
						regs[RDX].state = RS_UNKONWN;
						break;
					case POPQ:
						regs[regid_of(opec.src)].state = RS_UNKONWN;
						regs[RSP].state = RS_UNKONWN;
						break;
					case PUSHQ:
						regs[RSP].state = RS_UNKONWN;
						break;
					default:
						BOOST_ASSERT(false);
						// break;
				}
			} else if (opec.mne == CALL) {
				breakRegsInfo(regs);
			} else if (opec.mne == LABEL) {
				// Clear all register value.
				for (auto& r: regs)
					r.state = RS_UNKONWN;
			}

		} else { // no param
			switch (opec.mne) {
				case SYSCALL: 
					breakRegsInfo(regs);
					break;
				case REP_MOVSQ: case REP_MOVSL:
				case REP_MOVSW: case REP_MOVSB:
					regs[RSI].state = RS_UNKONWN;
					regs[RDI].state = RS_UNKONWN;
					regs[RCX].state = RS_UNKONWN;
					break;
				default:
					break;
			}
		}
	}
}

void asmOptimize(vector<PlnOpeCode> &opecodes)
{
	auto opec = opecodes.begin();
	while (opec != opecodes.end()) {
		if (opec->mne == ADDQ) {
			BOOST_ASSERT(opec->dst->type == OP_REG);
			auto src = opec->src;
			if (src->type == OP_IMM) {
				int64_t i = int64_of(src);	
				if (i==0) {
					opec->mne = MNE_NONE;
					continue;
				} else if (i==1) {
					opec->mne = INCQ;
					opec->src = opec->dst;
					opec->dst = NULL;
					delete src;
				} else if (i==-1) {
					opec->mne = DECQ;
					opec->src = opec->dst;
					opec->dst = NULL;
					delete src;
				}
			}
		} else if (opec->mne == SUBQ) {
			BOOST_ASSERT(opec->dst->type == OP_REG);
			auto src = opec->src;
			if (src->type == OP_IMM) {
				int64_t i = int64_of(src);	
				if (i==0) {
					opec->mne = MNE_NONE;
					continue;
				} else if (i==1) {
					opec->mne = DECQ;
					opec->src = opec->dst;
					opec->dst = NULL;
					delete src;
				}
			}
		} 
		opec++;
	}
}

