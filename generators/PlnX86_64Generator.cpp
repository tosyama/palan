/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include <vector>
#include <iostream>
#include <string>
#include <string.h>
#include <stdio.h>
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

enum GenEttyType {
	GA_CODE,
	GA_REG,
	GA_MEM
};

#define CREATE_CHECK_FLAG(f)	bool is_##f##_mem = f->type == GA_MEM;\
							 	bool is_##f##_reg = f->type == GA_REG;\
							 	bool is_##f##_code = f->type == GA_CODE;\
							 	bool is_##f##_sint = f->data_type == DT_SINT;\
							 	bool is_##f##_uint = f->data_type == DT_UINT;\
							 	bool is_##f##_flo = f->data_type == DT_FLOAT;

//static PlnX86_64RegisterMachine m;

// Register operand (e.g. %rax)
inline PlnRegOperand* reg(int regid, int size=8) { return new PlnRegOperand(regid, size); }

inline int regid_of(const PlnGenEntity *e) {
	return regid_of(e->ope);
}

inline bool is_greg(int regid) {
	return regid <= R15;
}

// Immediate operand (e.g. $10)
inline PlnImmOperand* imm(int64_t value) { return new PlnImmOperand(value); }

inline int64_t int64_of(const PlnGenEntity *e) {
	return int64_of(e->ope);
}

// Addressing mode(Memory access) operand (e.g. -8($rax))
inline PlnAdrsModeOperand* adrs(int base_regid, int displacement=0, int index_regid=-1, int scale=0) {
	return new PlnAdrsModeOperand(base_regid, displacement, index_regid, scale);
}

// label operand (e.g. $.LC1)
inline PlnLabelOperand* lbl(const string &label, int id=-1) {
	BOOST_ASSERT(id >= -1);
	return new PlnLabelOperand(label, id);
}

// label with addressing mode operand (e.g. .LC1(%rip))
inline PlnLabelAdrsModeOperand* lblval(const string &label, int base_regid = RIP) {
	return new PlnLabelAdrsModeOperand(label, base_regid);
}

inline PlnOperandInfo* ope(const PlnGenEntity* e) {
	return e->ope->clone();
}

static bool opecmp(const PlnOperandInfo *l, const PlnOperandInfo *r)
{
	if (l->type != r->type) return false;
	switch (l->type) {
		case OP_REG: 
			{
				auto ll = static_cast<const PlnRegOperand*>(l);
				auto rr = static_cast<const PlnRegOperand*>(r);
				return ll->regid == rr->regid
					&& ll->size == rr->size;
			}
		case OP_ADRS:
			{
				auto ll = static_cast<const PlnAdrsModeOperand*>(l);
				auto rr = static_cast<const PlnAdrsModeOperand*>(r);
				return ll->base_regid == rr->base_regid
					&& ll->displacement == rr->displacement
					&& ll->index_regid == rr->index_regid
					&& ll->scale == rr->scale;
			}
		case OP_IMM: 
			{
				return int64_of(l) == int64_of(r);
			}
		case OP_LBL: 
			BOOST_ASSERT(false);
		case OP_LBLADRS: 
			BOOST_ASSERT(false);
		default:
			BOOST_ASSERT(false);
	}
}

PlnX86_64Generator::PlnX86_64Generator(ostream& ostrm)
	: PlnGenerator(ostrm), require_align(false), max_const_id(0), func_stack_size(0)
{
}

PlnX86_64Generator::~PlnX86_64Generator()
{
	for (ConstInfo& ci: const_buf) {
		if (!ci.size)
			delete ci.data.str;
		if (ci.comment)
			delete ci.comment;
	}
}

int PlnX86_64Generator::registerFlo64Const(const PlnOperandInfo* constValue) {
	BOOST_ASSERT(constValue->type == OP_IMM);

	union {
		int64_t i;
		double d;
	} u;

	u.i = int64_of(constValue);

	for (ConstInfo& ci: const_buf) {
		if (ci.size == 8 && ci.data.i == u.i) {
			if (ci.id < 0) {
				ci.id = max_const_id;
				max_const_id++;
			}
			return ci.id;
		}
	}
	
	ConstInfo cinfo(8, 8, u.i);
	cinfo.id = max_const_id;
	max_const_id++;
	cinfo.comment = new string(to_string(u.d));
	const_buf.push_back(cinfo);

	return cinfo.id;
}

static int calcNextAlign(int cur, int next)
{
	cur += next;
	if (cur & 1)
		return 1;
	if (cur & 2)
		return 2;
	if (cur & 4)
		return 4;
	return 8;
}

int PlnX86_64Generator::registerConstData(vector<PlnRoData> &rodata)
{
	vector<ConstInfo> cinfs;
	for (auto data: rodata) {
		int64_t val;
		if (data.data_type == DT_SINT || data.data_type == DT_UINT) {
			val = data.val.i;
		} else if (data.data_type == DT_FLOAT) {
			if (data.size == 4) {
				union {
					int32_t i;
					float f;
				} u;
				u.f = data.val.f;
				val = u.i;

			} else if (data.size == 8) {
				union {
					int64_t i;
					double f;
				} u;
				u.f = data.val.f;
				val = u.i;

			} else
				BOOST_ASSERT(false);
		} else
			BOOST_ASSERT(false);

		ConstInfo cinfo(data.size, data.alignment, val);
		cinfs.push_back(cinfo);
	}
	
	if (const_buf.size()) {
		int align_i = 0;
		int generated_i = const_buf[0].generated;
		for (int i=0; i<const_buf.size(); i++) {
			if (align_i < const_buf[i].alignment || generated_i != const_buf[i].generated) {
				align_i = const_buf[i].alignment;
				generated_i = const_buf[i].generated;
			}

			if (align_i >= cinfs[0].alignment) {
				if ((const_buf.size() - i) < cinfs.size())
					break;
				int j=i;
				int generated = const_buf[i].generated;
				bool matched = true;
				bool first = true;
				for (auto& cinf: cinfs) {
					if (generated != const_buf[j].generated
							|| cinf.data.i != const_buf[j].data.i
							|| cinf.size != const_buf[j].size
							|| (!first && cinf.alignment != const_buf[j].alignment)) {
						matched = false;
						break;
					}
					if (first) first = false;
					j++;
				}
				if (matched) {
					if (const_buf[i].id < 0) {
						const_buf[i].id = max_const_id;
						max_const_id++;
					}
					return const_buf[i].id;
				}
			}
		}
	}

	// add comment for float
	BOOST_ASSERT(cinfs.size() == rodata.size());
	for (int i=0; i<rodata.size(); ++i) {
		if (rodata[i].data_type == DT_FLOAT) {
			cinfs[i].comment = new string(to_string(rodata[i].val.f));
		}
	}

	// register new const values
	cinfs[0].id = max_const_id;
	max_const_id++;
	const_buf.insert(const_buf.end(), cinfs.begin(), cinfs.end());

	return cinfs[0].id;
}

int PlnX86_64Generator::registerString(string& str)
{
	for (ConstInfo& ci: const_buf) {
		if (ci.size == 0 && (*ci.data.str == str)) {
			BOOST_ASSERT(ci.id >= 0);
			return ci.id;
		}
	}

	ConstInfo cinfo(max_const_id, new string(str));
	const_buf.push_back(cinfo);

	max_const_id++;
	return const_buf.back().id;
}

void PlnX86_64Generator::comment(const string s)
{
	m.push(COMMENT,NULL,NULL,s);
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
	if (label == "") {
		m.push(LABEL, lbl("_start"));
		require_align = true;
	} else {
		m.push(LABEL, lbl(label));
	}
}

void PlnX86_64Generator::genJumpLabel(int id, string comment)
{
	m.push(LABEL, lbl(".L", id), NULL, comment);
}

void PlnX86_64Generator::genJump(int id, string comment)
{
	m.push(JMP, lbl(".L", id), NULL, comment);
}

void PlnX86_64Generator::genTrueJump(int id, int cmp_type, string comment)
{
	PlnX86_64Mnemonic jcmd;
	switch (cmp_type) {
		case CMP_EQ: jcmd = JE; break;
		case CMP_NE: jcmd = JNE; break;
		case CMP_L: jcmd = JL; break;
		case CMP_G: jcmd = JG; break;
		case CMP_LE: jcmd = JLE; break;
		case CMP_GE: jcmd = JGE; break;
		case CMP_B: jcmd = JB; break;
		case CMP_A: jcmd = JA; break;
		case CMP_BE: jcmd = JBE; break;
		case CMP_AE: jcmd = JAE; break;

		defalt:
			BOOST_ASSERT(false);
	}
	m.push(jcmd, lbl(".L", id), NULL, comment);
}

void PlnX86_64Generator::genFalseJump(int id, int cmp_type, string comment)
{
	PlnX86_64Mnemonic jcmd;
	switch (cmp_type) {
		case CMP_EQ: jcmd = JNE; break;
		case CMP_NE: jcmd = JE; break;
		case CMP_L: jcmd = JGE; break;
		case CMP_G: jcmd = JLE; break;
		case CMP_LE: jcmd = JG; break;
		case CMP_GE: jcmd = JL; break;
		case CMP_B: jcmd = JAE; break;
		case CMP_A: jcmd = JBE; break;
		case CMP_BE: jcmd = JA; break;
		case CMP_AE: jcmd = JB; break;

		defalt:
			BOOST_ASSERT(false);
	}
	m.push(jcmd, lbl(".L", id), NULL, comment);
}

void PlnX86_64Generator::genEntryFunc()
{
	m.push(PUSHQ, reg(RBP));
	m.push(MOVQ, reg(RSP), reg(RBP));
	func_stack_size = 0;
}

void PlnX86_64Generator::genLocalVarArea(int size)
{
	BOOST_ASSERT((size%8) == 0 && size >= 0);

	if (require_align) {
		if ((size-8) % 16)
			size = size + 8;
		require_align = false;

	} else {
		if (!size) return;
		if ((size) % 16)
			size = size + 8;
	}

	func_stack_size = size;
	m.push(SUBQ, imm(size), reg(RSP));
}

void PlnX86_64Generator::genEndFunc()
{
	m.popOpecodes(os);

	int alignment = 1;
	for (ConstInfo &ci: const_buf) {
		if (!ci.generated) {
			if (alignment < ci.alignment) {
				os << "	.align " << int(ci.alignment) << endl;
				alignment = ci.alignment;
			}
			if (ci.id >= 0)
				os << ".LC" << ci.id << ":" << endl;

			if (ci.size == 8) {
				os << "	.quad	" << ci.data.i;
				alignment = calcNextAlign(alignment, 8);
			} else if (ci.size == 4) {
				os << "	.long	" << ci.data.i;
				alignment = calcNextAlign(alignment, 4);
			} else if (ci.size == 2) {
				os << "	.short	" << ci.data.i;
				alignment = calcNextAlign(alignment, 2);
			} else if (ci.size == 1) {
				os << "	.byte	" << ci.data.i;
				alignment = calcNextAlign(alignment, 1);
			} else if (ci.size == 0) {	// string
				string ostr = replace_all_copy(*ci.data.str,"\n","\\n");
				os << "	.string \"" << ostr << "\"";
				alignment = 1;
			} else
				BOOST_ASSERT(false);

			if (ci.comment)
				os << "\t# " << *ci.comment ;
			os << endl;
			
			ci.generated++;
		}
	}
}

void PlnX86_64Generator::genSaveReg(int regid, PlnGenEntity* dst)
{
	m.push(MOVQ, reg(regid), ope(dst));
}

void PlnX86_64Generator::genLoadReg(int regid, PlnGenEntity* src)
{
	m.push(MOVQ, ope(src), reg(regid));
}

void PlnX86_64Generator::genCCall(string& cfuncname, vector<int> &arg_dtypes, bool has_va_arg)
{
	if (has_va_arg) {
		int flo_cnt = 0;
		for (auto dt: arg_dtypes)
			if (dt == DT_FLOAT)
				flo_cnt++;

		if (flo_cnt) {
			if (flo_cnt > 8)
				flo_cnt = 8;
			m.push(MOVQ, imm(flo_cnt), reg(RAX));
		} else {
			m.push(XORQ, reg(RAX), reg(RAX));
		}
	}
	m.push(CALL, lbl(cfuncname));
}

void PlnX86_64Generator::genSysCall(int id, const string& comment)
{
	m.push(MOVQ, imm(id), reg(RAX), comment);
	m.push(SYSCALL);
}

void PlnX86_64Generator::genReturn()
{
	if (func_stack_size)
		m.push(ADDQ, imm(func_stack_size), reg(RSP));
	m.push(POPQ, reg(RBP));	
	m.push(RET);
}

void PlnX86_64Generator::genMainReturn()
{
	m.push(MOVQ, reg(RBP), reg(RSP));
	m.push(POPQ, reg(RBP));
	m.push(XORQ, reg(RDI), reg(RDI));
	m.push(MOVQ, imm(60), reg(RAX));
	m.push(SYSCALL);
}

void PlnX86_64Generator::moveMemToReg(const PlnGenEntity* mem, int regid)
{
	BOOST_ASSERT(mem->type == GA_MEM);
	PlnX86_64Mnemonic mnemonic;
	int regsize = 8;

	if (mem->data_type == DT_SINT) {
		switch (mem->size) {
			case 1: mnemonic = MOVSBQ; break;
			case 2: mnemonic = MOVSWQ; break;
			case 4: mnemonic = MOVSLQ; break;
			case 8: mnemonic = MOVQ; break;
		}
	} else { // unsigned
		switch (mem->size) {
			case 1: mnemonic = MOVZBQ; break;
			case 2: mnemonic = MOVZWQ; break;
			case 4: mnemonic = MOVL; regsize = 4; break;
			case 8: mnemonic = MOVQ; break;
		}
	}

	m.push(mnemonic, ope(mem), reg(regid, regsize));
}

static PlnOperandInfo* adjustImmediateFloat(const PlnGenEntity* src, int dst_size)
{
	BOOST_ASSERT(src->type == GA_CODE);

	if (dst_size == 4) {
		union { int64_t i; double d; } tmp;
		union { uint32_t i; float f; } u;
		if (src->data_type == DT_FLOAT) {
			tmp.i = int64_of(src);
			u.f = tmp.d;	// double -> float

		} else {
			if (src->data_type == DT_SINT)
				u.f = int64_of(src);	// int -> float
			else if (src->data_type == DT_UINT)
				u.f = static_cast<uint64_t>(int64_of(src));	// int -> float
			else
				BOOST_ASSERT(false);
		}
		static_cast<PlnImmOperand*>(src->ope)->value = u.i;
		return imm(u.i);

	 } else {
		BOOST_ASSERT(dst_size == 8);
		if (src->data_type != DT_FLOAT) {
			union { uint64_t i; double d; } u;
			if (src->data_type == DT_SINT)
				u.d = int64_of(src);	// int -> double
			else if (src->data_type == DT_UINT)
				u.d = static_cast<uint64_t>(int64_of(src));	// int -> float
			else
				BOOST_ASSERT(false);

			static_cast<PlnImmOperand*>(src->ope)->value = u.i;
			return imm(u.i);
		}
		return ope(src);
	}
}

static void adjustImmediateEntity(const PlnGenEntity* src, int dst_data_type, int dst_size) {
	BOOST_ASSERT(src->type == GA_CODE);
	PlnImmOperand *imm = static_cast<PlnImmOperand *>(src->ope);
	PlnGenEntity* editable_src = const_cast<PlnGenEntity*>(src);

	if (src->data_type == DT_SINT || src->data_type == DT_UINT) {
		// integer -> integer: do nothing.
		if (dst_data_type == DT_SINT || dst_data_type == DT_UINT)
			return;

		BOOST_ASSERT(dst_data_type == DT_FLOAT);
		if (dst_size == 4) {
			// convert int -> flo32
			union { int64_t i; float f; } u;
			if (src->data_type == DT_SINT)
				u.f = imm->value;
			else if (src->data_type == DT_UINT)
				u.f = static_cast<uint64_t>(imm->value);
			imm->value = u.i;
			editable_src->data_type = DT_FLOAT;
			return;
		} 

		BOOST_ASSERT(dst_size == 8);
		// convert int -> flo64
		union { int64_t i; double d; } u;
		if (src->data_type == DT_SINT)
			u.d = imm->value;
		else if (src->data_type == DT_UINT)
			u.d = static_cast<uint64_t>(imm->value);
		imm->value = u.i;
		editable_src->data_type = DT_FLOAT;
		return;

	} if (src->data_type == DT_FLOAT) {
		if (dst_data_type == DT_FLOAT) {
			// flo64 -> flo64: do nothing.
			if (dst_size == 8) return;

			// convert flo64 -> flo32
			BOOST_ASSERT(dst_size == 4);
			union { int64_t i; double d; } ud;
			union { int64_t i; float f; } uf;
			ud.i = imm->value;
			uf.f = ud.d;
			imm->value = uf.i;
			editable_src->data_type = DT_FLOAT;
			return;
		}

		BOOST_ASSERT(dst_data_type == DT_SINT || dst_data_type == DT_UINT);
		// convert flo64 -> integer
		union { int64_t i; double d; } u;
		u.i = imm->value;
		imm->value = u.d;
		editable_src->data_type = DT_SINT;
		return;
	}
	BOOST_ASSERT(false);
}

static bool needAbsCopy(const PlnOperandInfo* imm_ope)
{
	BOOST_ASSERT(imm_ope->type == OP_IMM);
	int64_t i = int64_of(imm_ope);

	return (i < -2147483648 || i > 4294967295);
}

enum {
	SMASK = 0xF000,

	SREGI = 0x1000,
	SMEMI = 0x2000,
	SREGU = 0x3000,
	SMEMU = 0x4000,
	SREGF = 0x5000,
	SMEMF = 0x6000,
	SXMMF = 0x7000,
	SIMMI = 0x8000,
	SIMMU = 0x9000,
	SIMMF = 0xA000,
	SBIGIMMI = 0xB000,
	SBIGIMMU = 0xC000,
	SBIGIMMF = 0xD000,

	SSZMASK = 0x0F00,
	S8 = 0x0800, S4 = 0x0400, S2 = 0x0200, S1 = 0x0100,

	DMASK = 0x00F0,

	DREGI = 0x0010,
	DMEMI = 0x0020,
	DREGU = 0x0030,
	DMEMU = 0x0040,
	DREGF = 0x0050,
	DMEMF = 0x0060,
	DXMMF = 0x0070,

	DSZMASK = 0x000F,
	D8 = 0x0008, D4 = 0x0004, D2 = 0x0002, D1 = 0x0001,
};

typedef struct {
	PlnX86_64Mnemonic mnem;
	int tmp_regid;
} GenInfo;

static int getOpePattern(const PlnGenEntity* dst, const PlnGenEntity* src)
{
	int pattern = 0;

	if (src->type == GA_REG) {
		pattern =	!is_greg(regid_of(src)) ? SXMMF:
					src->data_type == DT_FLOAT ? SREGF:
					src->data_type == DT_SINT ? SREGI: SREGU;

	} else if (src->type == GA_MEM) {
		pattern =	src->data_type == DT_FLOAT ? SMEMF:
					src->data_type == DT_SINT ? SMEMI: SMEMU;

	} else if (src->type == GA_CODE) {
		adjustImmediateEntity(src, dst->data_type, dst->size);
		if (needAbsCopy(src->ope)) {
			pattern =	src->data_type == DT_FLOAT ? SBIGIMMF:
						src->data_type == DT_SINT ? SBIGIMMI: SBIGIMMU;
		} else {
			pattern =	src->data_type == DT_FLOAT ? SIMMF:
						src->data_type == DT_SINT ? SIMMI: SIMMU;
		}

	} else
		BOOST_ASSERT(false);

	if (src->size == 8) pattern |= S8;
	else if (src->size == 4) pattern |= S4;
	else if (src->size == 2) pattern |= S2;
	else if (src->size == 1) pattern |= S1;
	
	if (dst->type == GA_REG) {
		pattern |=	!is_greg(regid_of(dst)) ? DXMMF:
					dst->data_type == DT_FLOAT ? DREGF:
					dst->data_type == DT_SINT ? DREGI: DREGU;

	} else if (dst->type == GA_MEM) {
		pattern |=	dst->data_type == DT_FLOAT ? DMEMF:
					dst->data_type == DT_SINT ? DMEMI: DMEMU;
	} else
		BOOST_ASSERT(false);
	
	if (dst->size == 8) pattern |= D8;
	else if (dst->size == 4) pattern |= D4;
	else if (dst->size == 2) pattern |= D2;
	else if (dst->size == 1) pattern |= D1;

	return pattern;
}

static PlnX86_64Mnemonic movIntRegToMne[4] = {MOVB, MOVW, MOVL, MOVQ};
static PlnX86_64Mnemonic movSintMemToMne[4] = {MOVSBQ, MOVSWQ, MOVSLQ, MOVQ};
static PlnX86_64Mnemonic movUintMemToMne[4] = {MOVZBQ, MOVZWQ, MOVL, MOVQ};

inline int srcByte(int pattern)
{
	if (pattern & S1) return 0;
	if (pattern & S2) return 1;
	if (pattern & S4) return 2;
	if (pattern & S8) return 3;
}

inline int dstByte(int pattern)
{
	if (pattern & D1) return 0;
	if (pattern & D2) return 1;
	if (pattern & D4) return 2;
	if (pattern & D8) return 3;
}

static int maskIntSize(int pattern)
{
	int pat = pattern;
	switch (pattern & SMASK) {
		case SREGI: case SREGU:
		case SMEMI: case SMEMU:
		case SIMMI: case SIMMU:
		case SBIGIMMI: case SBIGIMMU:
			pat = pat & (SMASK|DMASK|DSZMASK);
			break;
		default:
			break;
	};
	switch (pattern & DMASK) {
		case DREGI: case DREGU:
		case DMEMI: case DMEMU:
			pat = pat & (SMASK|SSZMASK|DMASK);
			break;
		default:
			break;
	};
	return pat;
}

static int setMove2GenInfo(int pattern, GenInfo genInfos[3])
{
	switch (maskIntSize(pattern)) {
	// float -> float
		// 1. xmm8f->xmm8f: MOVSD
		// 2. xmm8f->mem8f: MOVSD
		// 3. xmm8f->reg8f: MOVQ
		// 4. xmm8f->xmm4f: CVTSD2SS
		// 5. xmm8f->mem4f: CVTSD2SS(X11) + MOVSS
		// 6. xmm8f->reg4f:  CVTSD2SS(X11) + MOVQ
		case SXMMF|S8 + DXMMF|D8:	// 1
		case SXMMF|S8 + DMEMF|D8:	// 2
			genInfos[0] = {MOVSD, 0};
			return 1;
		case SXMMF|S8 + DREGF|D8:	// 3
			BOOST_ASSERT(false);
		case SXMMF|S8 + DXMMF|D4:	// 4
			BOOST_ASSERT(false);
		case SXMMF|S8 + DMEMF|D4:	// 5
			genInfos[0] = {CVTSD2SS, XMM11};
			genInfos[1] = {MOVSS, 0};
			return 2;
		case SXMMF|S8 + DREGF|D4:	// 6
			BOOST_ASSERT(false);

		// 1. xmm4f->xmm4f: MOVSS
		// 2. xmm4f->mem4f: MOVSS
		// 3. xmm4f->reg4f: MOVQ
		// 4. xmm4f->xmm8f: CVTSS2SD
		// 5. xmm4f->mem8f: CVTSS2SD(X11) + MOVSD
		// 6. xmm4f->reg8f: CVTSS2SD(X11) + MOVQ
		case SXMMF|S4 + DXMMF|D4:
		case SXMMF|S4 + DMEMF|D4:
		case SXMMF|S4 + DREGF|D4:
		case SXMMF|S4 + DXMMF|D8:
		case SXMMF|S4 + DMEMF|D8:
		case SXMMF|S4 + DREGF|D8:
			BOOST_ASSERT(false);

		// 1. mem8f->xmm8f: MOVSD
		// 2. mem8f->mem8f: MOVQ(R11) + MOVQ
		// 3. mem8f->reg8f: MOVQ
		// 4. mem8f->xmm4f: MOVSD(X11) + CVTSD2SS
		// 5. mem8f->mem4f: MOVSD(X11) + CVTSD2SS(X11) + MOVSS
		// 6. mem8f->reg4f: MOVSD(X11) + CVTSD2SS(X11) + MOVQ
		case SMEMF|S8 + DXMMF|D8:	// 1
			genInfos[0] = {MOVSD, 0};
			return 1;
		case SMEMF|S8 + DMEMF|D8:	// 2
			genInfos[0] = {MOVQ, R11};
			genInfos[1] = {MOVQ, 0};
			return 2;
		case SMEMF|S8 + DREGF|D8:	// 3
		case SMEMF|S8 + DXMMF|D4:	// 4
			BOOST_ASSERT(false);
		case SMEMF|S8 + DMEMF|D4:	// 5
			genInfos[0] = {MOVSD, XMM11};
			genInfos[1] = {CVTSD2SS, XMM11};
			genInfos[2] = {MOVSS, 0};
			return 3;
		case SMEMF|S8 + DREGF|D4:	// 6
			BOOST_ASSERT(false);

		// 1. mem4f->xmm8f: MOVSS(X11) + CVTSS2SD
		// 2. mem4f->mem8f: MOVSS(X11) + CVTSS2SD(X11) + MOVSD
		// 3. mem4f->reg8f: MOVSS(X11) + CVTSS2SD(X11) + MOVQ
		// 4. mem4f->xmm4f: MOVSS
		// 5. mem4f->mem4f: MOVL(R11) + MOVL
		// 6. mem4f->reg4f: MOVSD(X11) + CVTSD2SS(X11) + MOVQ
		case SMEMF|S4 + DXMMF|D8:	// 1
			genInfos[0] = {MOVSS, XMM11};
			genInfos[1] = {CVTSS2SD, 0};
			return 2;
		case SMEMF|S4 + DMEMF|D8:	// 2
			genInfos[0] = {MOVSS, XMM11};
			genInfos[1] = {CVTSS2SD, XMM11};
			genInfos[2] = {MOVSD, 0};
			return 3;
		case SMEMF|S4 + DREGF|D8:	// 3
		case SMEMF|S4 + DXMMF|D4:	// 4
			BOOST_ASSERT(false);
		case SMEMF|S4 + DMEMF|D4:	// 5
			genInfos[0] = {MOVL, R11};
			genInfos[1] = {MOVL, 0};
			return 2;

		case SMEMF|S4 + DREGF|D4:	// 6
			BOOST_ASSERT(false);

		// 1. reg8f->xmm8f: MOVQ
		// 2. reg8f->mem8f: MOVQ
		// 3. reg8f->reg8f: MOVQ
		// 4. reg8f->xmm4f: MOVQ(X11) + CVTSD2SS
		// 5. reg8f->reg4f: MOVQ(X11) + CVTSD2SS(X11) + MOVQ
		// 6. reg8f->mem4f: MOVQ(X11) + CVTSD2SS(X11) + MOVSS

		// 1. reg4f->xmm4f: MOVQ
		// 2. reg4f->mem4f: MOVL
		// 3. reg4f->reg4f: MOVQ
		// 4. reg4f->xmm8f: MOVQ(X11) + CVTSS2SD 
		// 5. reg4f->mem8f: MOVQ(X11) + CVTSS2SD(X11) + MOVQ
		// 6. reg4f->reg8f: MOVQ(X11) + CVTSS2SD(X11) + MOVQ

	// float -> integer
		// 1. xmm8f->memNi: CVTTSD2SI(R11) + MOVn
		// 2. xmm8f->regNi: CVTTSD2SI
		// 3. xmm4f->memNi: CVTTSS2SI(R11) + MOVn
		// 4. xmm4f->regNi: CVTTSD2SI
		case SXMMF|S8 + DMEMI:
			genInfos[0] = {CVTTSD2SI, R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;
		case SXMMF|S8 + DREGI:
		case SXMMF|S4 + DMEMI:
		case SXMMF|S4 + DREGI:
			BOOST_ASSERT(false);

		// 1. mem8f->memNi: MOVSD(X11) + CVTTSD2SI(R11) + MOVn
		// 2. mem8f->regNi: MOVSD(X11) + CVTTSD2SI
		// 3. mem4f->memNi: MOVSS(X11) + CVTTSS2SI(R11) + MOVn
		// 4. mem4f->regNi: MOVSS(X11) + CVTTSS2SI
		// 1.
		case SMEMF|S8 + DMEMI:
		case SMEMF|S8 + DMEMU:
			genInfos[0] = {MOVSD, XMM11};
			genInfos[1] = {CVTTSD2SI, R11};
			genInfos[2] = {movIntRegToMne[dstByte(pattern)], 0};
			return 3;
		// 2.
		case SMEMF|S8 + DREGI:
		case SMEMF|S8 + DREGU:
			genInfos[0] = {MOVSD, XMM11};
			genInfos[1] = {CVTTSD2SI, 0};
			return 2;
		// 3.
		case SMEMF|S4 + DMEMI:
		case SMEMF|S4 + DMEMU:
			genInfos[0] = {MOVSS, XMM11};
			genInfos[1] = {CVTTSS2SI, R11};
			genInfos[2] = {movIntRegToMne[dstByte(pattern)], 0};
			return 3;
		// 4.
		case SMEMF|S4 + DREGI:
		case SMEMF|S4 + DREGU:
			genInfos[0] = {MOVSS, XMM11};
			genInfos[1] = {CVTTSS2SI, 0};
			return 2;

		// 1. reg8f->memNi: MOVQ(X11) + CVTTSD2SI(R11) + MOVn
		// 2. reg8f->regNi: MOVQ(X11) + CVTTSD2SI
		// 3. reg4f->memNi: MOVQ(X11) + CVTTSS2SI(R11) + MOVn
		// 4. reg4f->regNi: MOVQ(X11) + CVTTSS2SI

	// integer -> float
		// 1. memNi->xmm8f: MOVn(R11) + CVTSI2SD
		// 2. memNi->mem8f: MOVn(R11) + CVTSI2SD(X11) + MOVSD
		// 3. memNi->reg8f: MOVn(R11) + CVTSI2SD(X11) + MOVQ
		// 4. memNi->xmm4f: MOVn(R11) + CVTSI2SS
		// 5. memNi->mem4f: MOVn(R11) + CVTSI2SS(X11) + MOVSS
		// 6. memNi->reg4f: MOVn(R11) + CVTSI2SS(X11) + MOVQ
		// 1.
		case SMEMI + DXMMF|D8:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SD, 0};
			return 2;
		case SMEMU + DXMMF|D8:
			genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SD, 0};
			return 2;
		// 2.
		case SMEMI + DMEMF|D8:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SD, XMM11};
			genInfos[2] = {MOVSD, 0};
			return 3;
		case SMEMU + DMEMF|D8:
			genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SD, XMM11};
			genInfos[2] = {MOVSD, 0};
			return 3;

		// 4.
		case SMEMI + DXMMF|D4:
			BOOST_ASSERT(false);

		// 5.
		case SMEMI + DMEMF|D4:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SS, XMM11};
			genInfos[2] = {MOVSS, 0};
			return 3;
		case SMEMU + DMEMF|D4:
			genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SS, XMM11};
			genInfos[2] = {MOVSS, 0};
			return 3;

		// 1. regNi->xmm8f: CVTTSI2SD
		// 2. regNi->mem8f: CVTTSI2SD(X11) + MOVSD
		// 3. regNi->reg8f: CVTTSI2SD(X11) + MOVQ
		// 4. regNi->xmm4f: CVTTSI2SS
		// 5. regNi->mem4f: CVTTSI2SS(X11) + MOVSS
		// 6. regNi->reg4f: CVTTSI2SS(X11) + MOVQ
		// 1.
		case SREGI + DXMMF|D8:
			BOOST_ASSERT(false);
		// 2.
		case SREGI + DMEMF|D8:
			genInfos[0] = {CVTSI2SD, XMM11};
			genInfos[1] = {MOVSD, 0};
			return 2;
		case SREGU + DMEMF|D8:
			BOOST_ASSERT(false);
		// 4.
		case SREGI + DXMMF|D4:
			BOOST_ASSERT(false);
		// 5.
		case SREGI + DMEMF|D4:
			genInfos[0] = {CVTSI2SS, XMM11};
			genInfos[1] = {MOVSS, 0};
			return 2;
		case SREGU + DMEMF|D4:
			BOOST_ASSERT(false);

	// integer -> integer
		// 1. memNi->regNi: MOVn
		// 2. memNi->memNi: MOVn(R11) + MOVn
		// 3. regNi->memNi: MOVn
		// 4. regNi->regNi: MOVn
		// 1.
		case SMEMI + DREGI:
		case SMEMI + DREGU:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], 0};
			return 1;
		case SMEMU + DREGI:
		case SMEMU + DREGU:
			genInfos[0] = {movUintMemToMne[srcByte(pattern)], 0};
			return 1;
		// 2.
		case SMEMI + DMEMI:
		case SMEMI + DMEMU:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;
		case SMEMU + DMEMI:
		case SMEMU + DMEMU:
			genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;
		// 3.
		case SREGI + DMEMI: case SREGI + DMEMU:
		case SREGU + DMEMI: case SREGU + DMEMU:
			genInfos[0] = {movIntRegToMne[dstByte(pattern)], 0};
			return 1;
		// 4.
		case SREGI + DREGI: case SREGI + DREGU:
		case SREGU + DREGI: case SREGU + DREGU:
			genInfos[0] = {MOVQ, 0};
			return 1;

	// immediate -> integer
		// 1. imm -> regNi: MOVQ
		// 2. imm -> memNi: MOVn
		// 3. bigimm -> regNi: MOVABSQ
		// 4. bigimm -> memNi: MOVABSQ(R11) + MOVn
		// 1.
		case SIMMI + DREGI: case SIMMI + DREGU:
		case SIMMU + DREGI: case SIMMU + DREGU:
			genInfos[0] = {MOVQ, 0};
			return 1;
		// 2.
		case SIMMI + DMEMI: case SIMMI + DMEMU:
		case SIMMU + DMEMI: case SIMMU + DMEMU:
			genInfos[0] = {movIntRegToMne[dstByte(pattern)], 0};
			return 1;
		// 3.
		case SBIGIMMI + DREGI: case SBIGIMMI + DREGU:
		case SBIGIMMU + DREGI: case SBIGIMMU + DREGU:
			genInfos[0] = {MOVABSQ, 0};
			return 1;
		// 4.
		case SBIGIMMI + DMEMI: case SBIGIMMI + DMEMU:
		case SBIGIMMU + DMEMI: case SBIGIMMU + DMEMU:
			genInfos[0] = {MOVABSQ, R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;

	// immediate -> float
		// 1. imm -> xmm8f/xmm4f: MOVQ(R11) + MOVQ
		// 2. imm -> mem8f/mem4f: MOVQ(R11) + MOVn
		// 3. bigimm -> xmm8/xmm4: MOVABSQ(R11) + MOVQ
		// 4. bigimm -> mem8f/mem4f: MOVABSQ(R11) + MOVn
		// 1.
		case SIMMF|S8 + DXMMF|D8:
		case SIMMF|S8 + DXMMF|D4:
			genInfos[0] = {MOVQ, R11};
			genInfos[1] = {MOVQ, 0};
			return 2;
		// 2.
		case SIMMF|S8 + DMEMF|D8:
		case SIMMF|S8 + DMEMF|D4:
			genInfos[0] = {MOVQ, R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;
		// 3.
		case SBIGIMMF|S8 + DXMMF|D8:
		case SBIGIMMF|S8 + DXMMF|D4:
			genInfos[0] = {MOVABSQ, R11};
			genInfos[1] = {MOVQ, 0};
			return 2;
		// 4.
		case SBIGIMMF|S8 + DMEMF|D8:
		case SBIGIMMF|S8 + DMEMF|D4:
			genInfos[0] = {MOVABSQ, R11};
			genInfos[1] = {movIntRegToMne[dstByte(pattern)], 0};
			return 2;
	};

	return 0;
}

static void pushGenInfo(PlnX86_64RegisterMachine& m, GenInfo *ginf, int num, const PlnGenEntity* dst, const PlnGenEntity* src, string& comment)
{
	PlnOperandInfo* src_ope = ope(src);
	PlnOperandInfo* dst_ope;
	string temp;
	for (int i=0; i<num; i++) {
		dst_ope = ginf->tmp_regid ? reg(ginf->tmp_regid) : ope(dst);

		if (ginf->mnem == MOVB) {
			if (src_ope->type == OP_REG)
				static_cast<PlnRegOperand*>(src_ope)->size = 1;

		} else if (ginf->mnem == MOVW) {
			if (dst_ope->type == OP_REG);
				static_cast<PlnRegOperand*>(dst_ope)->size = 2;
			if (src_ope->type == OP_REG);
				static_cast<PlnRegOperand*>(src_ope)->size = 2;

		} else if (ginf->mnem == MOVL) {
			if (dst_ope->type == OP_REG)
				static_cast<PlnRegOperand*>(dst_ope)->size = 4;
			if (src_ope->type == OP_REG)
				static_cast<PlnRegOperand*>(src_ope)->size = 4;

		} else if (ginf->mnem == ADDQ
				&& src_ope->type == OP_IMM && int64_of(src)==1) {
			ginf->mnem = INCQ;
			src_ope = dst_ope;
			dst_ope = NULL;

		} else if (ginf->mnem == SUBQ
				&& src_ope->type == OP_IMM && int64_of(src)==1) {
			ginf->mnem = DECQ;
			src_ope = dst_ope;
			dst_ope = NULL;

		} else if (ginf->mnem == DIVQ) {
			m.push(MOVQ, imm(0), reg(RDX));
			dst_ope = NULL;

		} else if (ginf->mnem == IDIVQ) {
			m.push(CQTO);
			dst_ope = NULL;
		}

		m.push(ginf->mnem, src_ope, dst_ope);
		if (ginf->tmp_regid) {
			src_ope = reg(ginf->tmp_regid);
		}
		ginf++;
	}
	m.addComment(comment);
}

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	if (opecmp(dst->ope, src->ope))
		return;	// do nothing.

	int pattern = getOpePattern(dst, src);
	GenInfo genInfo[3];
	int n = setMove2GenInfo(pattern, genInfo);
	BOOST_ASSERT(n>0);

	pushGenInfo(m, genInfo, n, dst, src, comment);
}

void PlnX86_64Generator::genLoadAddress(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	BOOST_ASSERT(src->type == GA_MEM);
	BOOST_ASSERT(dst->size == 8);
	int regid = R11;
	if (dst->type == GA_REG) {
		regid = regid_of(dst);
	}
	m.push(LEA, ope(src), reg(regid), comment);

	if (dst->type == GA_MEM) {
		m.push(MOVQ, reg(regid), ope(dst));
	}
}

typedef enum {
	CALC_ADD,
	CALC_SUB,
	CALC_MUL,
	CALC_DIV
} CalcOperation;

static int setNumCalc2GenInfo(CalcOperation calc, int pattern, GenInfo genInfos[3])
{
	BOOST_ASSERT(pattern & D8);

	PlnX86_64Mnemonic fmne;
	PlnX86_64Mnemonic imne;
	if (calc == CALC_ADD) {
		fmne = ADDSD; imne = ADDQ;
	} else if (calc == CALC_SUB) {
		fmne = SUBSD; imne = SUBQ;
	} else if (calc  == CALC_MUL) {
		fmne = MULSD; imne = IMULQ;
	} else {
		BOOST_ASSERT(calc == CALC_DIV);
		fmne = DIVSD;
	} 
	
	// target + second
	switch (maskIntSize(pattern)) {
	// float + float
		// 1. xmm8f + xmm8f: ADDSD
		// 2. xmm8f + xmm4f: CVTSS2SD(X11) + ADDSD
		// 3. xmm8f + mem8f: ADDSD
		// 4. xmm8f + mem4f: MOVSS(X11) + CVTSS2SD(X11) + ADDSD
		// 5. xmm8f + reg8f:
		// 6. xmm8f + reg4f:
		case DXMMF|D8 + SXMMF|S8:
		case DXMMF|D8 + SXMMF|S4:
			BOOST_ASSERT(false);
		case DXMMF|D8 + SMEMF|S8:
			genInfos[0] = {fmne, 0};
			return 1;
		case DXMMF|D8 + SMEMF|S4:
			genInfos[0] = {MOVSS, XMM11};
			genInfos[1] = {CVTSS2SD, XMM11};
			genInfos[2] = {fmne, 0};
			return 3;
		case DXMMF|D8 + SREGF|S8:
		case DXMMF|D8 + SREGF|S4:
			BOOST_ASSERT(false);

	// float + integer
		// 1. xmm8f + memNi: MOVn(R11) + CVTSI2SD(X11) + ADDSD
		// 2. xmm8f + regNi:
		// 1.
		case DXMMF|D8 + SMEMI:
			genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
			genInfos[1] = {CVTSI2SD, XMM11};
			genInfos[2] = {fmne, 0};
			return 3;
		case DXMMF|D8 + SMEMU:
			BOOST_ASSERT(false);
		// 2.
		case DXMMF|D8 + SREGI:
		case DXMMF|D8 + SREGU:
			BOOST_ASSERT(false);

	// float + immediate
		// 1. xmm8f + imm: MOVQ(R11) + MOVQ(X11) + ADDSD
		// 2. xmm8f + bigimm: MOVABSQ(R11) + MOVQ(X11) + ADDSD
		// 1.
		case DXMMF|D8 + SIMMF|S8:
			genInfos[0] = {MOVQ, R11};
			genInfos[1] = {MOVQ, XMM11};
			genInfos[2] = {fmne, 0};
			return 3;
		// 2.
		case DXMMF|D8 + SBIGIMMF|S8:
			genInfos[0] = {MOVABSQ, R11};
			genInfos[1] = {MOVQ, XMM11};
			genInfos[2] = {fmne, 0};
			return 3;

	}

	// integer + integer
	if (calc == CALC_DIV) {
		switch (pattern) {
		// uinteger + uinteger
			// 1. reg8u / immu: MOVQ(R11) + DIVQ(MOVQ 0,RDX + DIVQ)
			// 2. reg8u / bigimmu: MOVABSQ(R11) + DIVQ(MOVQ 0,RDX + DIVQ)
			// 3. reg8u / memNu: MOVNi(R11) + DIVQ(MOVQ 0,RDX + DIVQ)
			// 4. reg8u / regNu: ?
			// 1.
			case DREGU|D8 + SIMMU|S8:
				genInfos[0] = {MOVQ, R11};
				genInfos[1] = {DIVQ, 0};
				return 2;
			// 2.
			case DREGU|D8 + SBIGIMMU|S8:
				genInfos[0] = {MOVABSQ, R11};
				genInfos[1] = {DIVQ, 0};
				return 2;
			// 3.
			case DREGU|D8 + SMEMU|S8: case DREGU|D8 + SMEMU|S4:
			case DREGU|D8 + SMEMU|S2: case DREGU|D8 + SMEMU|S1:
				genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
				genInfos[1] = {DIVQ, 0};
				return 2;
			// 4.
			case DREGU|D8 + SREGU|S8: case DREGU|D8 + SREGU|S4:
			case DREGU|D8 + SREGU|S2: case DREGU|D8 + SREGU|S1:
				BOOST_ASSERT(false);
		}

		switch (maskIntSize(pattern)) {
		// integer + integer
			// 1. reg8i / immi: MOVQ(R11) + IDIVQ(CQTO + IDIVQ))
			// 2. reg8i / bigimmi: MOVABSQ(R11) + IDIVQ(CQTO + IDIVQ))
			// 3. reg8i / memNi: MOVNi(R11) + DIVQ(MOVQ 0,RDX + DIVQ)
			// 4. reg8u / regNu: ?
			// 1.
			case DREGI + SIMMI: case DREGI + SIMMU:
			case DREGU + SIMMI:
				genInfos[0] = {MOVQ, R11};
				genInfos[1] = {IDIVQ, 0};
				return 2;
			// 2.
			case DREGI + SBIGIMMI: case DREGI + SBIGIMMU:
			case DREGU + SBIGIMMI:
				genInfos[0] = {MOVABSQ, R11};
				genInfos[1] = {IDIVQ, 0};
				return 2;
			// 3.
			case DREGI + SMEMI:
			case DREGU + SMEMI:
				genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
				genInfos[1] = {IDIVQ, 0};
				return 2;
			case DREGI + SMEMU:
				genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
				genInfos[1] = {IDIVQ, 0};
				return 2;
			// 4.
			case DREGI + SREGI:
			case DREGU + SREGI:
			case DREGI + SREGU:
				BOOST_ASSERT(false);
		}

	} else {
		switch (pattern) {
			// 1. reg8i + imm: ADDQ
			// 2. reg8i + bigimm: ?
			// 3. reg8i + mem8i: ADDQ
			// 4. reg8i + mem4-1: MOVNi(R11) + ADDQ

			// 1.
			case DREGI|D8 + SIMMI|S8: case DREGI|D8 + SIMMU|S8:
			case DREGU|D8 + SIMMI|S8: case DREGU|D8 + SIMMU|S8:
				genInfos[0] = {imne, 0};
				return 1;
			// 2.
			case DREGI|D8 + SBIGIMMI|S8: case DREGI|D8 + SBIGIMMU|S8:
			case DREGU|D8 + SBIGIMMI|S8: case DREGU|D8 + SBIGIMMU|S8:
				BOOST_ASSERT(false);

			// 3.
			case DREGI|D8 + SMEMI|S8:
			case DREGI|D8 + SMEMU|S8:
				genInfos[0] = {imne, 0};
				return 1;

			// 4.
			case DREGI|D8 + SMEMI|S4: case DREGI|D8 + SMEMI|S2: case DREGI|D8 + SMEMI|S1:
			case DREGU|D8 + SMEMI|S4: case DREGU|D8 + SMEMI|S2: case DREGU|D8 + SMEMI|S1:
				BOOST_ASSERT(!(pattern & S8));
				genInfos[0] = {movSintMemToMne[srcByte(pattern)], R11};
				genInfos[1] = {imne, 0};
				return 2;
			case DREGI|D8 + SMEMU|S4: case DREGI|D8 + SMEMU|S2: case DREGI|D8 + SMEMU|S1:
			case DREGU|D8 + SMEMU|S4: case DREGU|D8 + SMEMU|S2: case DREGU|D8 + SMEMU|S1:
				BOOST_ASSERT(!(pattern & S8));
				genInfos[0] = {movUintMemToMne[srcByte(pattern)], R11};
				genInfos[1] = {imne, 0};
				return 2;
		}
	}

	return 0;
}

static void genCalc(PlnX86_64RegisterMachine &m, CalcOperation calc, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment)
{
	int pattern = getOpePattern(tgt, scnd);
	GenInfo genInfo[3];
	int n = setNumCalc2GenInfo(calc, pattern, genInfo);
	BOOST_ASSERT(n>0);
	pushGenInfo(m, genInfo, n, tgt, scnd, comment);
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	genCalc(m, CALC_ADD, tgt, scnd, comment);
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	genCalc(m, CALC_SUB, tgt, scnd, comment);
}

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	genCalc(m, CALC_MUL, tgt, scnd, comment);
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	genCalc(m, CALC_DIV, tgt, scnd, comment);
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt, string comment)
{
	if (tgt->data_type == DT_FLOAT) {
		int64_t mask = 0x8000000000000000;
		m.push(MOVABSQ, imm(mask), reg(R11));
		m.push(MOVQ, reg(R11), reg(XMM11));
		m.push(XORPD, reg(XMM11), ope(tgt), comment);

	} else {
		m.push(NEGQ, ope(tgt), NULL, comment);
	}
}

void PlnX86_64Generator::genCmpImmFRegMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->type == GA_CODE);

	PlnOperandInfo* fst_ope = adjustImmediateFloat(first, second->size);
	if (needAbsCopy(fst_ope)) {
		m.push(MOVABSQ, fst_ope, reg(R11));
		m.push(MOVQ, reg(R11), reg(XMM11));
	} else {
		m.push(MOVQ, fst_ope, reg(R11));
		m.push(MOVQ, reg(R11), reg(XMM11));
	}

	if (second->size == 4) {
		m.push(UCOMISS, ope(second), reg(XMM11));
	} else {
		m.push(UCOMISD, ope(second), reg(XMM11));
	}
}

void PlnX86_64Generator::genCmpFMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->size <= second->size);
	if (first->size < second->size) {
		BOOST_ASSERT(second->size == 8);
		m.push(CVTSS2SD, ope(first), reg(XMM11));
	} else if (second->size == 4) {
		m.push(MOVSS, ope(first), reg(XMM11, 4));
	} else {
		BOOST_ASSERT(first->size == 8 && second->size == 8);
		m.push(MOVSD, ope(first), reg(XMM11));
	}

	if (second->size == 4) {
		m.push(UCOMISS, ope(second), reg(XMM11));
	} else {
		m.push(UCOMISD, ope(second), reg(XMM11));
	}
}

void PlnX86_64Generator::genCmpFRegFMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->size == 8);

	if (second->size == 4) {
		m.push(CVTSS2SD, ope(second), reg(XMM11));
		m.push(UCOMISD, reg(XMM11), ope(first));
	} else { 
		m.push(UCOMISD, ope(second), ope(first));
	}
}

void PlnX86_64Generator::genCmpIMemFRegMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	int ireg_id;
	if (first->type == GA_MEM) {
		moveMemToReg(first, R11);
		ireg_id = R11;
	} else {
		ireg_id = regid_of(first);
	}

	if (second->size == 4) {
		m.push(CVTSI2SS, reg(ireg_id), reg(XMM11,4));
		m.push(UCOMISS, ope(second), reg(XMM11,4));

	} else {
		m.push(CVTSI2SD, reg(ireg_id), reg(XMM11));
		m.push(UCOMISD, ope(second), reg(XMM11));
	}
}

void PlnX86_64Generator::genCmpIRegMemFImm(const PlnGenEntity* first, const PlnGenEntity* second)
{
	PlnOperandInfo *scnd_ope = adjustImmediateFloat(second, 8);
	int id = registerFlo64Const(scnd_ope);
	delete scnd_ope;	// don't use any more.

	if (first->size == 8 || first->type == GA_REG) {
		m.push(CVTSI2SD, ope(first), reg(XMM11));

	} else {
		moveMemToReg(first, R11);
		m.push(CVTSI2SD, reg(R11), reg(XMM11));
	}

	m.push(UCOMISD, lblval(".LC" + to_string(id)), reg(XMM11));
}

int PlnX86_64Generator::genCmpI2F(const PlnGenEntity* first, const PlnGenEntity* second, int cmp_type)
{
	CREATE_CHECK_FLAG(first);
	CREATE_CHECK_FLAG(second);

	if (is_first_code && (is_second_reg || is_second_mem)) {
		genCmpImmFRegMem(first, second);

	} else if ((is_first_reg || is_first_mem) && is_second_code) {
		genCmpIRegMemFImm(first, second);

	} else if ((is_first_reg || is_first_mem) && (is_second_reg || is_second_mem)) {
		genCmpIMemFRegMem(first, second);

	} else {
		BOOST_ASSERT(false);
	}
	switch (cmp_type) {
		case CMP_L: cmp_type = CMP_B; break;
		case CMP_G: cmp_type = CMP_A; break;
		case CMP_LE: cmp_type = CMP_BE; break;
		case CMP_GE: cmp_type = CMP_AE; break;
		}
	return cmp_type;
}

int rev_cmp(int cmp_type) {
	switch (cmp_type) {
		case CMP_L: return CMP_G;
		case CMP_G: return CMP_L;
		case CMP_LE: return CMP_GE;
		case CMP_GE: return CMP_LE;
	}
	return cmp_type;
}

int PlnX86_64Generator::genCmp(PlnGenEntity* first, PlnGenEntity* second, int cmp_type, string comment)
{
	CREATE_CHECK_FLAG(first);
	CREATE_CHECK_FLAG(second);
	
	BOOST_ASSERT(!(is_first_code && is_second_code));

	if (is_first_flo && is_second_flo) {
		// Float comparison
		// ucomisd 2nd, 1st - G/A:1st > 2nd, L/B:1st < 2nd
		// ucomisd reg, reg
		// ucomisd mem, reg

		if (is_first_code && (is_second_reg || is_second_mem)) {
			genCmpImmFRegMem(first, second);

		} else if ((is_first_reg || is_first_mem) && is_second_code) {
			genCmpImmFRegMem(second, first);
			cmp_type = rev_cmp(cmp_type);

		} else if (is_first_mem && is_second_mem) {
			if (first->size <= second->size) {
				genCmpFMem(first, second);
			} else {
				genCmpFMem(second, first);
				cmp_type = rev_cmp(cmp_type);
			}

		} else if (is_first_reg && is_second_mem) {
			genCmpFRegFMem(first, second);

		} else if (is_first_mem && is_second_reg) {
			genCmpFRegFMem(second, first);
			cmp_type = rev_cmp(cmp_type);

		// currentlly no case
		// } else if (is_first_reg && is_second_reg) {
		// 	BOOST_ASSERT(first->size == 8);
		// 	BOOST_ASSERT(second->size == 8);
		//	m.push(UCOMISD, ope(second), ope(first));

		} else {
			BOOST_ASSERT(false);
		}

		m.addComment(comment);

		switch (cmp_type) {
			case CMP_L: cmp_type = CMP_B; break;
			case CMP_G: cmp_type = CMP_A; break;
			case CMP_LE: cmp_type = CMP_BE; break;
			case CMP_GE: cmp_type = CMP_AE; break;
		}
		return cmp_type;
	}

	if ((is_first_sint || is_first_uint) && is_second_flo) {
		cmp_type =  genCmpI2F(first, second, cmp_type);
		m.addComment(comment);
		return cmp_type;
	}

	if (is_first_flo && (is_second_sint || is_second_uint)) {
		cmp_type =  genCmpI2F(second, first, rev_cmp(cmp_type));
		m.addComment(comment);
		return cmp_type;
	}

	// Integer comparison
	// cmp 2nd, 1st -  G/A:1st > 2nd, L/B:1st < 2nd
	//  cmp reg, reg
	//  cmp reg, mem  // reg(mem_min), mem
	//  cmp code, reg
	//  cmp code, mem
	if ((second->type != GA_CODE && first->type == GA_CODE) 
			|| (second->type == GA_MEM && first->type != GA_MEM)
			|| (second->type == GA_MEM && first->type == GA_MEM
				&& second->size > first->size)) {
		// swap
		auto tmp = second;
		second = first;
		first = tmp;
		cmp_type = rev_cmp(cmp_type);
	}

	if (first->data_type == DT_UINT && second->data_type == DT_UINT) {
		switch (cmp_type) {
			case CMP_L: cmp_type = CMP_B; break;
			case CMP_G: cmp_type = CMP_A; break;
			case CMP_LE: cmp_type = CMP_BE; break;
			case CMP_GE: cmp_type = CMP_AE; break;
		}
	}
	BOOST_ASSERT(first->type != GA_CODE);

	PlnOperandInfo* scnd_ope;
	if (second->type == GA_REG) {
		scnd_ope = reg(regid_of(second), first->size);

	} else if (second->type == GA_MEM) {
		moveMemToReg(second, R11);
		scnd_ope = reg(R11, first->size);
	} else {
		scnd_ope = ope(second);
	}

	PlnX86_64Mnemonic mnemonic;
	if (first->type == GA_MEM) {
		switch (first->size) {
			case 1: mnemonic = CMPB; break;
			case 2: mnemonic = CMPW; break;
			case 4: mnemonic = CMPL; break;
			case 8: mnemonic = CMPQ; break;
			default:
				BOOST_ASSERT(false);
		}
	} else {
		mnemonic = CMP;
	}

	m.push(mnemonic, scnd_ope, ope(first), comment);

	return cmp_type;
}

int PlnX86_64Generator::genMoveCmpFlag(PlnGenEntity* tgt, int cmp_type, string comment)
{
	BOOST_ASSERT(tgt->type == GA_REG);
	BOOST_ASSERT(tgt->size == 8);

	PlnX86_64Mnemonic setcmd =
		cmp_type == CMP_EQ ? SETE:
		cmp_type == CMP_NE ? SETNE:
		cmp_type == CMP_L ? SETL:
		cmp_type == CMP_G ? SETG:
		cmp_type == CMP_LE ? SETLE:
		cmp_type == CMP_GE ? SETGE:
		cmp_type == CMP_B ? SETB:
		cmp_type == CMP_A ? SETA:
		cmp_type == CMP_BE ? SETBE:
		cmp_type == CMP_AE ? SETAE:
		COMMENT;

	BOOST_ASSERT(setcmd != COMMENT);
	
	m.push(setcmd, reg(regid_of(tgt),1), NULL, comment);
	m.push(MOVZBQ, reg(regid_of(tgt),1), ope(tgt));
}

void PlnX86_64Generator::genNullClear(vector<unique_ptr<PlnGenEntity>> &refs)
{
	if (refs.size() == 1) {
		m.push(MOVQ, imm(0), ope(refs[0].get()));

	} else if (refs.size() >= 2) {
		m.push(XORQ, reg(RAX), reg(RAX));
		
		for (auto& r: refs) {
			m.push(MOVQ, reg(RAX), ope(r.get()));
		}
	}
}

void PlnX86_64Generator::genMemCopy(int cp_unit, string& comment)
{
	PlnX86_64Mnemonic mnemonic;

	if (cp_unit == 8) {
		mnemonic = REP_MOVSQ;
	} else if (cp_unit == 4) {
		mnemonic = REP_MOVSL;
	} else if (cp_unit == 2) {
		mnemonic = REP_MOVSW;
	} else {
		BOOST_ASSERT(cp_unit == 1);
		mnemonic = REP_MOVSB;
	}

	m.push(CLD);
	m.push(mnemonic, NULL, NULL, comment);
}

unique_ptr<PlnGenEntity> PlnX86_64Generator::getEntity(PlnDataPlace* dp)
{
	BOOST_ASSERT(dp->data_type != DT_UNKNOWN);
	unique_ptr<PlnGenEntity> e(new PlnGenEntity());
	if (dp->type == DP_STK_BP) {
		e->type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->ope = adrs(RBP, dp->data.stack.offset);

	} else if (dp->type == DP_STK_SP) {
		e->type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->ope = adrs(RSP, dp->data.stack.offset);

	} else if (dp->type == DP_REG) {
		e->type = GA_REG;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->ope = reg(dp->data.reg.id, dp->size);

	} else if (dp->type == DP_INDRCT_OBJ) {
		e->type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->ope = adrs(
			dp->data.indirect.base_id,
			dp->data.indirect.displacement,
			dp->data.indirect.index_id,
			dp->size);

	} else if (dp->type == DP_LIT_INT || dp->type == DP_LIT_FLO) {
		e->type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->ope = imm(dp->data.intValue);

	} else if (dp->type == DP_RO_STR) {
		int id;
		BOOST_ASSERT(dp->data.rostr);
		id = registerString(*dp->data.rostr);
		 
		e->type = GA_MEM;
		e->size = 8;
		e->data_type = DT_OBJECT_REF;
		e->ope = lbl("$.LC", id);

	} else if (dp->type == DP_RO_DATA) {
		int id;
		id = registerConstData(*(dp->data.rodata));
		e->type = GA_MEM;
		e->size = 8;
		e->data_type = DT_OBJECT_REF;
		e->ope = lbl("$.LC", id);
	
	} else if (dp->type == DP_GLBL) {
		e->type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->ope = lblval(*dp->data.varName);

	} else
		BOOST_ASSERT(false);

	return e;
}

