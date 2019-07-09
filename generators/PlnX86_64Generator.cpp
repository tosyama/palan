/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
#include "PlnX86_64RegisterMachine.h"

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

static PlnX86_64RegisterMachine m;

// Register operand (e.g. %rax)
inline PlnRegOperand* reg(int regid, int size=8) { return new PlnRegOperand(regid, size); }

inline int regid_of(const PlnGenEntity *e) {
	return regid_of(e->ope);
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
	: PlnGenerator(ostrm), require_align(false)
{
}

int PlnX86_64Generator::registerFlo64Const(const PlnOperandInfo* constValue) {
	BOOST_ASSERT(constValue->type == OP_IMM);
	ConstInfo cinfo;
	cinfo.generated = false;
	cinfo.type = GCT_FLO64;
	cinfo.data.q = int64_of(constValue);

	int id=0;
	for (ConstInfo ci: const_buf) {
		if (ci.type == cinfo.type && ci.data.d == cinfo.data.d) {
			return id;
		}
		id++;
	}

	const_buf.push_back(cinfo);
	return id; 
}

template <typename T>
int findCinfo(T*& arr, PlnGenConstType type, vector<int64_t> &int_array, vector<PlnX86_64Generator::ConstInfo> &const_buf)
{
	int id = 0;
	for (auto cinfo: const_buf) {
		if (cinfo.type == type && cinfo.size == int_array.size()) {
			T* darr = (T*)cinfo.data.q_arr;
			int i;
			for (i=0; i<cinfo.size; i++)
				if (darr[i] != int_array[i])
					break;
			if (i==cinfo.size) {
				arr = NULL;
				return id; // found same data xxto id;
			}
		}
		id++;
	}

	arr = new T[int_array.size()];
	for (int i=0; i<int_array.size(); i++)
		arr[i] = int_array[i];
	
	return id;
}

int PlnX86_64Generator::registerConstArray(vector<int64_t> &int_array, int item_size)
{
	ConstInfo cinfo;
	cinfo.generated = false;
	cinfo.size = int_array.size();
	int id;

	if (item_size == 8) {
		cinfo.type = GCT_INT64_ARRAY;
		id = findCinfo(cinfo.data.q_arr, cinfo.type, int_array, const_buf);

	} else if (item_size == 4) {
		cinfo.type = GCT_INT32_ARRAY;
		id = findCinfo(cinfo.data.l_arr, cinfo.type, int_array, const_buf);

	} else if (item_size == 2) {
		cinfo.type = GCT_INT16_ARRAY;
		id = findCinfo(cinfo.data.s_arr, cinfo.type, int_array, const_buf);

	} else if (item_size == 1) {
		cinfo.type = GCT_INT8_ARRAY;
		id = findCinfo(cinfo.data.b_arr, cinfo.type, int_array, const_buf);

	} else
		BOOST_ASSERT(false);	

	if (cinfo.data.q_arr)
		const_buf.push_back(cinfo);

	return id;
}

int PlnX86_64Generator::registerString(string& str)
{
	int id = 0;
	for (auto cinfo: const_buf) {
		if (cinfo.type == GCT_STRING) {
			if ((*cinfo.data.str) == str)
				return id;
		}
		id++;
	}

	ConstInfo cinfo;
	cinfo.type = GCT_STRING;
	cinfo.generated = false;
	cinfo.data.str = new string(str);
	const_buf.push_back(cinfo);

	return id;
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
			break;

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

	m.push(SUBQ, imm(size), reg(RSP));
}

void PlnX86_64Generator::genEndFunc()
{
	m.popOpecodes(os);

	int i = 0;
	for (ConstInfo &ci: const_buf) {
		if (!ci.generated) {
			os << "	.align 8" << endl;
			os << ".LC" << i << ":" << endl;
			if (ci.type == GCT_FLO64) {
				os << "	.quad	" << ci.data.q << "	# " << ci.data.d << endl;
			} else if (ci.type == GCT_INT64_ARRAY) {
				for (int i=0; i<ci.size; i++) {
					os << "	.quad	" << ci.data.q_arr[i] << endl;
				}
			} else if (ci.type == GCT_INT32_ARRAY) {
				for (int i=0; i<ci.size; i++) {
					os << "	.long	" << ci.data.l_arr[i] << endl;
				}
			} else if (ci.type == GCT_INT16_ARRAY) {
				for (int i=0; i<ci.size; i++) {
					os << "	.short	" << ci.data.s_arr[i] << endl;
				}
			} else if (ci.type == GCT_INT8_ARRAY) {
				for (int i=0; i<ci.size; i++) {
					os << "	.byte	" << int(ci.data.b_arr[i]) << endl;
				}
			} else if (ci.type == GCT_STRING) {
				string ostr = replace_all_copy(*ci.data.str,"\n","\\n");
				os << "	.string \"" << ostr << "\"" << endl;
			} else {
				BOOST_ASSERT(false);
			}
			ci.generated = true;
		}
		i++;
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


void PlnX86_64Generator::genCCall(string& cfuncname, vector<int> &arg_dtypes, int va_arg_start)
{
	if (va_arg_start >= 0) {
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
	m.push(LEAVE);
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

void PlnX86_64Generator::moveRegTo(int regid, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->type == GA_MEM || dst->type == GA_REG);
	PlnX86_64Mnemonic mnemonic;

	switch (dst->size) {
		case 1: mnemonic = MOVB; break;
		case 2: mnemonic = MOVW; break;
		case 4: mnemonic = MOVL; break;
		case 8: mnemonic = MOVQ; break;
		default:
			BOOST_ASSERT(false);
	}

	m.push(mnemonic, reg(regid, dst->size), ope(dst));
}

static PlnOperandInfo* adjustImmediateInt(const PlnGenEntity* src)
{
	BOOST_ASSERT(src->type == GA_CODE);
	if (src->data_type == DT_FLOAT) {
		union { int64_t i; double f; } u;
		u.i = int64_of(src);
		return imm(u.f);
	}
	return ope(src);
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

static bool needAbsCopy(const PlnOperandInfo* imm_ope)
{
	BOOST_ASSERT(imm_ope->type == OP_IMM);
	int64_t i = int64_of(imm_ope);

	return (i < -2147483648 || i > 4294967295);
}

void PlnX86_64Generator::genMoveFReg(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	// flo(reg) -> int/uint;
	if (dst->data_type == DT_SINT || dst->data_type == DT_UINT) {
		BOOST_ASSERT(src->data_type == DT_FLOAT);
		BOOST_ASSERT(src->size == 8);

		if (dst->type == GA_REG) {
			BOOST_ASSERT(false);
		} else {
			m.push(CVTTSD2SI, ope(src), reg(R11));
			moveRegTo(R11, dst);
		}
		return;
	}

	BOOST_ASSERT(dst->data_type == DT_FLOAT);
	BOOST_ASSERT(dst->size == 4 || dst->size == 8);

	// flo -> flo
	if (src->data_type == DT_FLOAT) {
		BOOST_ASSERT(src->size == 4 || src->size == 8);

		if (src->size == 4) {
			// Currently no usecase size 4->4.
			BOOST_ASSERT(dst->size == 8);
			m.push(CVTSS2SD, ope(src), ope(dst));

		} else { // src->size == 8
			if (dst->size == 4) {
				m.push(CVTSD2SS, ope(src), reg(XMM11,4));
				m.push(MOVSS, reg(XMM11,4), ope(dst));

			} else { // dst->size == 8
				m.push(MOVSD, ope(src), ope(dst));
			}
		}
		return;
	}
	
	// int/uint -> flo
	PlnOperandInfo *src_ope;
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);
	BOOST_ASSERT(dst->size == 8);

	if (src->size == 8) {
		src_ope = ope(src);
	} else { 
		src_ope = reg(R11);
		moveMemToReg(src, R11);
	} 

	m.push(CVTSI2SD, src_ope, ope(dst));
}

void PlnX86_64Generator::genConvFMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	if (src->size == 4 && dst->size == 8) {
		m.push(CVTSS2SD, ope(src), reg(XMM11,4));
		m.push(MOVSD, reg(XMM11), ope(dst));

	} else {
		BOOST_ASSERT(src->size == 8 && dst->size == 4);
		m.push(CVTSD2SS, ope(src), reg(XMM11,4));
		m.push(MOVSS, reg(XMM11,4), ope(dst));
	}
}

void PlnX86_64Generator::genConvIRegMem2FMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->data_type == DT_FLOAT);
	BOOST_ASSERT(dst->size == 4 || dst->size == 8);
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);

	PlnOperandInfo *src_ope;
	if (src->size == 8) {
		src_ope = ope(src);
	} else {
		src_ope = reg(R11);
		moveMemToReg(src, R11);
	}

	if (dst->size == 8) {
		m.push(CVTSI2SD, src_ope, reg(XMM11));
		m.push(MOVSD, reg(XMM11), ope(dst));
	} else if (dst->size == 4) {
		m.push(CVTSI2SS, src_ope, reg(XMM11,4));
		m.push(MOVSS, reg(XMM11,4), ope(dst));
	}
}

void PlnX86_64Generator::genConvFMem2IMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		m.push(CVTTSS2SI, ope(src), reg(R11));
	} else {
		m.push(CVTTSD2SI, ope(src), reg(R11));
	}

	moveRegTo(R11, dst);
}

void PlnX86_64Generator::genConvFMem2IReg(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		m.push(CVTTSS2SI, ope(src), reg(regid_of(dst)));
	} else {
		m.push(CVTTSD2SI, ope(src), reg(regid_of(dst)));
	}
}

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	if (opecmp(dst->ope, src->ope))
		return;	// do nothing.

	CREATE_CHECK_FLAG(src);
	CREATE_CHECK_FLAG(dst);

	if (is_src_code) {
		PlnX86_64Mnemonic mnemonic;
		switch (dst->size) {
			case 1: mnemonic = MOVB; break;
			case 2: mnemonic = MOVW; break;
			case 4: mnemonic = MOVL; break;
			case 8: mnemonic = MOVQ; break;
		}

		PlnOperandInfo *src_ope;
		if (is_dst_sint || is_dst_uint) {
			src_ope = adjustImmediateInt(src);
		} else if (is_dst_flo) {
			src_ope = adjustImmediateFloat(src, dst->size);
		} else {
			src_ope = ope(src);
		}

		if (!needAbsCopy(src_ope)) {
			if (is_dst_reg && is_dst_flo) {
				m.push(MOVQ, src_ope, reg(R11));
				m.push(MOVQ, reg(R11), ope(dst));
			} else {
				m.push(mnemonic, src_ope, ope(dst));
			}

		} else if (is_dst_mem || is_dst_reg && is_dst_flo) {
			m.push(MOVABSQ, src_ope, reg(R11));
			m.push(mnemonic, reg(R11, dst->size), ope(dst));

		} else {
			BOOST_ASSERT(is_dst_reg);
			m.push(MOVABSQ, src_ope, ope(dst));
		}

	} else if (is_src_reg && is_src_flo || is_dst_reg && is_dst_flo) {
		genMoveFReg(src, dst);

	} else if (is_src_mem && is_src_flo && is_dst_mem && is_dst_flo
			&& src->size != dst->size) {
		genConvFMem(src, dst);

	} else if ((is_src_sint || is_src_uint) && is_dst_mem && is_dst_flo) {
		BOOST_ASSERT(is_src_reg || is_src_mem);
		genConvIRegMem2FMem(src, dst);

	} else if (is_src_mem && is_src_flo && is_dst_mem && (is_dst_sint || is_dst_uint)) {
		genConvFMem2IMem(src, dst);
	
	} else if (is_src_mem && is_src_flo && is_dst_reg && (is_dst_sint || is_dst_uint)) {
		genConvFMem2IReg(src, dst);

	}  else if (is_src_reg && is_dst_reg) {
		moveRegTo(regid_of(src), dst);

	} else if (is_src_mem && is_dst_reg) {
		moveMemToReg(src, regid_of(dst));

	} else if (is_src_mem && is_dst_mem) {
		moveMemToReg(src, R11);
		moveRegTo(R11, dst);

	} else {
		BOOST_ASSERT(is_src_reg && is_dst_mem);
		moveRegTo(regid_of(src), dst);
	}

	m.addComment(comment);
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

PlnOperandInfo* PlnX86_64Generator::genPreFloOperation(PlnGenEntity* tgt, PlnGenEntity* scnd)
{
	CREATE_CHECK_FLAG(tgt);
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(is_tgt_flo && is_tgt_reg);
	BOOST_ASSERT(tgt->size == 8);

	if (is_scnd_flo && is_scnd_mem) {
		if (scnd->size == 4) {
			m.push(CVTSS2SD, ope(scnd), reg(XMM11));
			return reg(XMM11);
		} else {
			return ope(scnd);
		}

	} else if (is_scnd_code) {
		PlnOperandInfo *scnd_ope = adjustImmediateFloat(scnd, tgt->size);

		if (needAbsCopy(scnd_ope)) {
			m.push(MOVABSQ, scnd_ope, reg(R11));
		} else {
			m.push(MOVQ, scnd_ope, reg(R11));
		}
		m.push(MOVQ, reg(R11), reg(XMM11));
		return reg(XMM11);

	} else if ((is_scnd_sint || is_scnd_uint) && is_scnd_mem) {
		moveMemToReg(scnd, R11);
		m.push(CVTSI2SD, reg(R11), reg(XMM11));
		return reg(XMM11);

	} else {
		BOOST_ASSERT(false);
	}
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(tgt->type != GA_MEM || !is_scnd_mem);

	if (tgt->data_type == DT_FLOAT) {
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd);
		m.push(ADDSD, scnd_ope, ope(tgt), comment);
		return;
	}

	if (is_scnd_code) {
		if (int64_of(scnd) == 1) {
			m.push(INCQ, ope(tgt), NULL, comment);
			return;
		}

		if (int64_of(scnd) < 0) {
			PlnGenEntity e;
			e.type = GA_CODE;
			e.size = 8;
			e.data_type = scnd->data_type;
			e.ope = imm(-int64_of(scnd));
			genSub(tgt, &e, comment);
			return;
		}
	}

	PlnOperandInfo* add_ope;
	if (is_scnd_mem && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		add_ope = reg(R11);
	} else
		add_ope = ope(scnd);
	
	m.push(ADDQ, add_ope, ope(tgt), comment);
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->type != GA_MEM || scnd->type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd);
		m.push(SUBSD, scnd_ope, ope(tgt), comment);
		return;
	}

	if (scnd->type == GA_CODE && int64_of(scnd) == 1) {
		m.push(DECQ, ope(tgt), NULL, comment);
		return;
	}

	PlnOperandInfo* sub_ope;
	if (scnd->type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		sub_ope = reg(R11);
	} else {
		sub_ope = ope(scnd);
	}

	m.push(SUBQ, sub_ope, ope(tgt), comment);
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

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->type != GA_MEM || scnd->type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd);
		m.push(MULSD, scnd_ope, ope(tgt), comment);
		return;
	}

	PlnOperandInfo* mul_ope;
	if (scnd->type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		mul_ope = reg(R11);
	} else {
		mul_ope = ope(scnd);
	}
	m.push(IMULQ, mul_ope, ope(tgt), comment);
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->type == GA_REG);
	if (tgt->data_type == DT_FLOAT) {
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd);
		m.push(DIVSD, scnd_ope, ope(tgt), comment);
		return;
	}

	BOOST_ASSERT(regid_of(tgt) == RAX);
	PlnOperandInfo* div_ope;
	if (scnd->type == GA_CODE) { 
		div_ope = reg(R11);
		if (needAbsCopy(scnd->ope)) {
			m.push(MOVABSQ, ope(scnd), reg(R11));
		} else {
			m.push(MOVQ, ope(scnd), reg(R11));
		}
	} else {
		div_ope = ope(scnd);
	}

	if (tgt->data_type == DT_UINT && scnd->data_type == DT_UINT) {
		m.push(MOVQ, imm(0), reg(RDX));
		m.push(DIVQ, div_ope);

	} else {
		m.push(CQTO);
		m.push(IDIVQ, div_ope);
	}
	m.addComment(comment);
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

	} else if (dp->type == DP_RO_DATA) {
		int id;
		if (dp->data.ro.int_array) {
			id = registerConstArray(*(dp->data.ro.int_array), dp->data.ro.item_size);

		} else if (dp->data.ro.flo_array) {
			vector<int64_t> int_array;

			if (dp->data.ro.item_size == 8) {
				union { double d; int64_t i; } data;
				for (double d: *(dp->data.ro.flo_array)) {
					data.d = d;
					int_array.push_back(data.i);
				}
			} else if (dp->data.ro.item_size == 4) {
				union { float f; int32_t i; } data;
				for (double d: *(dp->data.ro.flo_array)) {
					data.f = d;
					int_array.push_back(data.i);
				}

			} else {
				BOOST_ASSERT(false);
			}
			id = registerConstArray(int_array, dp->data.ro.item_size);

		} else {
			BOOST_ASSERT(dp->data.ro.str);
			id = registerString(*dp->data.ro.str);
		}
		e->type = GA_MEM;
		e->size = 8;
		e->data_type = DT_OBJECT_REF;
		e->ope = lbl("$.LC", id);

	} else
		BOOST_ASSERT(false);

	return e;
}

