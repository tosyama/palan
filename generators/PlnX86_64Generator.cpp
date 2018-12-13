/// x86-64 (Linux) assembly generator class definition.
///
/// @file	PlnX86_64Generator.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

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

enum GenEttyAllocType {
	GA_NULL,
	GA_CODE,
	GA_REG,
	GA_MEM
};

#define CREATE_CHECK_FLAG(f)	bool is_##f##_mem = f->alloc_type == GA_MEM;\
							 	bool is_##f##_reg = f->alloc_type == GA_REG;\
							 	bool is_##f##_code = f->alloc_type == GA_CODE;\
							 	bool is_##f##_sint = f->data_type == DT_SINT;\
							 	bool is_##f##_uint = f->data_type == DT_UINT;\
							 	bool is_##f##_flo = f->data_type == DT_FLOAT;

static const char* r(int rt, int size=8)
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

static const char* oprnd(const PlnGenEntity *e)
{
	if (e->type == GE_STRING) {
		return e->data.str->c_str();

	} else if (e->type == GE_INT) {
		if (e->alloc_type == GA_REG)
			return r(e->data.i, e->size);
		if (e->alloc_type == GA_CODE) {
			if (!e->buf) {
				e->buf = new char[32];
				sprintf(e->buf, "$%lld", (long long int)e->data.i);
			}
			return e->buf;
		}

	} else if (e->type == GE_FLO) {
		if (e->alloc_type == GA_CODE) {
			if (!e->buf) {
				e->buf = new char[32];
				sprintf(e->buf, "$%lld", (long long int)e->data.i);
			}
			return e->buf;
		}
	}
	
	BOOST_ASSERT(false);
}

PlnX86_64Generator::PlnX86_64Generator(ostream& ostrm)
	: PlnGenerator(ostrm), require_align(false)
{
}

int PlnX86_64Generator::registerConst(const PlnGenEntity* constValue) {
	BOOST_ASSERT(constValue->alloc_type == GA_CODE);
	ConstInfo cinfo;
	if (constValue->data_type == DT_FLOAT) {
		if (constValue->size == 8) {
			cinfo.generated = false;
			cinfo.type = GCT_FLO64;
			cinfo.data.d = constValue->data.f;
		} else
			BOOST_ASSERT(false);
	} else {
		BOOST_ASSERT(false);
	}

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

int PlnX86_64Generator::registerConstArray(vector<int64_t> &int_array, int item_size)
{
	ConstInfo cinfo;
	cinfo.generated = false;
	cinfo.size = int_array.size();
	if (item_size == 8) {
		cinfo.type = GCT_INT64_ARRAY;
		cinfo.data.q_arr = new int64_t[int_array.size()];
		for (int i=0; i<cinfo.size; i++)
			cinfo.data.q_arr[i] = int_array[i];

	} else if (item_size == 4) {
		cinfo.type = GCT_INT32_ARRAY;
		cinfo.data.l_arr = new int32_t[int_array.size()];
		for (int i=0; i<cinfo.size; i++)
			cinfo.data.l_arr[i] = int_array[i];

	} else if (item_size == 2) {
		cinfo.type = GCT_INT16_ARRAY;
		cinfo.data.s_arr = new int16_t[int_array.size()];
		for (int i=0; i<cinfo.size; i++)
			cinfo.data.s_arr[i] = int_array[i];

	} else if (item_size == 1) {
		cinfo.type = GCT_INT8_ARRAY;
		cinfo.data.b_arr = new int8_t[int_array.size()];
		for (int i=0; i<cinfo.size; i++)
			cinfo.data.b_arr[i] = int_array[i];

	} else
		BOOST_ASSERT(false);	

	int id = const_buf.size();
	const_buf.push_back(cinfo);
	return id;
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
		os << "_start:" << endl;
		require_align = true;
	} else
		os << label << ":" << endl;
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

	os << "	subq $" << size << ", %rsp" << endl;
}

void PlnX86_64Generator::genEndFunc()
{
	int i = 0;
	for (ConstInfo ci: const_buf) {
		if (!ci.generated) {
			os << "	.align 8" << endl;
			os << ".LD" << i << ":" << endl;
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
			} else {
				BOOST_ASSERT(false);
			}
		}
		i++;
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
			os << "	movq $" << flo_cnt << ", %rax" << endl;
		} else
			os << "	xorq %rax, %rax" << endl;
	}
	os << "	call " << cfuncname << endl;
}

void PlnX86_64Generator::genSysCall(int id, const string& comment)
{
	os << "	movq $" << id << ", %rax	# " <<  comment << endl;
	os << "	syscall" << endl;
}

void PlnX86_64Generator::genReturn()
{
	os << "	leave" << endl;
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

void PlnX86_64Generator::moveRegTo(int reg, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->alloc_type == GA_MEM || dst->alloc_type == GA_REG);
	string dst_safix;
	switch (dst->size) {
		case 1: dst_safix = "b"; break;
		case 2: dst_safix = "w"; break;
		case 4: dst_safix = "l"; break;
		case 8: dst_safix = "q"; break;
		default:
			BOOST_ASSERT(false);
	}
	os << "	mov" << dst_safix << " " << r(reg, dst->size) << ", " << oprnd(dst);
}

static void adjustImmediateInt(const PlnGenEntity* src)
{
	BOOST_ASSERT(src->alloc_type == GA_CODE);
	if (src->buf) return;
	if (src->type == GE_FLO) {
		src->data.i = src->data.f;
	}
}

static void adjustImmediateFloat(const PlnGenEntity* src, int dst_size)
{
	BOOST_ASSERT(src->alloc_type == GA_CODE);
	BOOST_ASSERT(src->buf == NULL);

	if (dst_size == 4) {
		union { uint32_t i; float f; } u;
		if (src->type == GE_FLO) {
			u.f = src->data.f;	// double -> float

		} else {
			BOOST_ASSERT(src->type == GE_INT);
			if (src->data_type == DT_SINT)
				u.f = src->data.i;	// int -> float
			else if (src->data_type == DT_UINT)
				u.f = static_cast<uint64_t>(src->data.i);	// int -> float
			else
				BOOST_ASSERT(false);
		}
		src->data.i = u.i;

	} else {
		BOOST_ASSERT(dst_size == 8);
		union { uint64_t i; double d; } u;
		if (src->type == GE_INT) {
			if (src->data_type == DT_SINT)
				u.d = src->data.i;	// int -> double
			else if (src->data_type == DT_UINT)
				u.d = static_cast<uint64_t>(src->data.i);	// int -> float
			else
				BOOST_ASSERT(false);

			src->data.i = u.i;
		}
	}
}

static bool needAbsCopy(const PlnGenEntity* immediate)
{
	BOOST_ASSERT(immediate->alloc_type == GA_CODE && (immediate->type == GE_INT || immediate->type == GE_FLO));

	if (immediate->data.i < -2147483648 || immediate->data.i > 4294967295) {
		return true;
	}
	return false;
}

void PlnX86_64Generator::genMoveFReg(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	// flo(reg) -> int/uint;
	if (dst->data_type == DT_SINT || dst->data_type == DT_UINT) {
		BOOST_ASSERT(src->data_type == DT_FLOAT);
		BOOST_ASSERT(src->size == 8);

		if (dst->alloc_type == GA_REG) {
			BOOST_ASSERT(false);
		} else {
			os << "	cvttsd2si " << oprnd(src) << ", " << r(R11, 8) << endl;
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
			os << "	cvtss2sd " << oprnd(src) << ", " << oprnd(dst);

		} else { // src->size == 8
			if (dst->size == 4) {
				os << "	cvtsd2ss " << oprnd(src) << ", " << r(XMM11, 4) << endl;
				os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);

			} else { // dst->size == 8
				os << "	movsd " << oprnd(src) << ", " << oprnd(dst);
			}
		}
		return;
	}
	
	// int/uint -> flo
	const char *src_str;
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);
	BOOST_ASSERT(dst->size == 8);

	if (src->size == 8) {
		src_str = oprnd(src);

	} else { 
		src_str = r(R11, 8);
		moveMemToReg(src, R11);
		os << endl;
	} 

	os << "	cvtsi2sd " << src_str << ", " << oprnd(dst);
}

void PlnX86_64Generator::genConvFMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	if (src->size == 4 && dst->size == 8) {
		os << "	cvtss2sd " << oprnd(src) << ", " << r(XMM11, 4) << endl;
		os << "	movsd	" << r(XMM11, 8) << ", " << oprnd(dst);

	} else {
		BOOST_ASSERT(src->size == 8 && dst->size == 4);
		os << "	cvtsd2ss " << oprnd(src) << ", " << r(XMM11, 4) << endl;
		os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);
	}
}

void PlnX86_64Generator::genConvIMem2FMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->data_type == DT_FLOAT);
	BOOST_ASSERT(dst->size == 4 || dst->size == 8);
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);
	const char *src_str;

	if (src->size == 8) {
		src_str = oprnd(src);
	} else {
		src_str = r(R11, 8);
		moveMemToReg(src, R11);
		os << endl;
	}

	if (dst->size == 8) {
		os << "	cvtsi2sd " << src_str << ", " << r(XMM11, 8) << endl;
		os << "	movsd	" << r(XMM11, 8) << ", " << oprnd(dst);
	} else if (dst->size == 4) {
		os << "	cvtsi2ss " << src_str << ", " << r(XMM11, 4) << endl;
		os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);
	}
}

void PlnX86_64Generator::genConvFMem2IMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		os << "	cvttss2si " << oprnd(src) << ", " << r(R11, 8) << endl;
	} else {
		os << "	cvttsd2si " << oprnd(src) << ", " << r(R11, 8) << endl;
	}

	moveRegTo(R11, dst);
}

void PlnX86_64Generator::genConvFMem2IReg(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		os << "	cvttss2si " << oprnd(src) << ", " << r(dst->data.i, 8);
	} else {
		os << "	cvttsd2si " << oprnd(src) << ", " << r(dst->data.i, 8);
	}
}

void PlnX86_64Generator::genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	if (dst->alloc_type == src->alloc_type) {
		if (!strcmp(oprnd(dst), oprnd(src))) return;	// do nothing
	}

	CREATE_CHECK_FLAG(src);
	CREATE_CHECK_FLAG(dst);

	string dst_safix;
	switch (dst->size) {
		case 1: dst_safix = "b"; break;
		case 2: dst_safix = "w"; break;
		case 4: dst_safix = "l"; break;
		case 8: dst_safix = "q"; break;
		default:
			BOOST_ASSERT(false);
	}

	if (is_src_code) {
		if (is_dst_sint || is_dst_uint)
			adjustImmediateInt(src);
		else if (is_dst_flo) {
			adjustImmediateFloat(src, dst->size);
		}

		if (!needAbsCopy(src)) {
			if (is_dst_reg && is_dst_flo) {
				os << "	movq " << oprnd(src) << ", " << r(R11, 8) << endl;
				os << "	movq	" << r(R11, 8) << ", " << oprnd(dst);
			} else {
				os << "	mov" << dst_safix << " " << oprnd(src) << ", " << oprnd(dst);
			}

		} else if (is_dst_mem || is_dst_reg && is_dst_flo) {
			os << "	movabsq " << oprnd(src) << ", " << r(R11, 8);
			os << endl;
			os << "	mov" << dst_safix << " " << r(R11, dst->size) << ", " << oprnd(dst);

		} else {
			BOOST_ASSERT(is_dst_reg);
			os << "	movabsq " << oprnd(src) << ", " << oprnd(dst);
		}

	} else if (is_src_reg && is_src_flo || is_dst_reg && is_dst_flo) {
		genMoveFReg(src, dst);

	} else if (is_src_mem && is_src_flo && is_dst_mem && is_dst_flo
			&& src->size != dst->size) {
		genConvFMem(src, dst);

	} else if (is_src_mem && (is_src_sint || is_src_uint) && is_dst_mem && is_dst_flo) {
		genConvIMem2FMem(src, dst);

	} else if (is_src_mem && is_src_flo && is_dst_mem && (is_dst_sint || is_dst_uint)) {
		genConvFMem2IMem(src, dst);
	
	} else if (is_src_mem && is_src_flo && is_dst_reg && (is_dst_sint || is_dst_uint)) {
		genConvFMem2IReg(src, dst);

	}  else if (is_src_reg && is_dst_reg) {
		moveRegTo(src->data.i, dst);

	} else if (is_src_mem && is_dst_reg) {
		moveMemToReg(src, dst->data.i);

	} else if (is_src_mem && is_dst_mem) {
		moveMemToReg(src, R11);
		os << endl;
		moveRegTo(R11, dst);

	} else {
		BOOST_ASSERT(is_src_reg && is_dst_mem);
		moveRegTo(src->data.i, dst);
	}

	if (comment != "") os << "	# " << comment;
	os << endl;
}

void PlnX86_64Generator::genLoadAddress(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	BOOST_ASSERT(src->alloc_type == GA_MEM);
	BOOST_ASSERT(dst->size == 8);
	int regid = R11;
	if (dst->alloc_type == GA_REG) {
		regid = dst->data.i;
	}
	os << "	lea " << oprnd(src) << ", " << r(regid, 8) << "	# " << comment << endl;

	if (dst->alloc_type == GA_MEM) {
		os << "	movq " << r(regid, 8) << ", " << oprnd(dst) << endl;
	}
}

const char* PlnX86_64Generator::genPreFloOperation(PlnGenEntity* tgt, PlnGenEntity* scnd)
{
	CREATE_CHECK_FLAG(tgt);
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(is_tgt_flo && is_tgt_reg);
	BOOST_ASSERT(tgt->size == 8);

	if (is_scnd_flo && is_scnd_mem) {
		if (scnd->size == 4) {
			os << "	cvtss2sd " << oprnd(scnd) << ", " << r(XMM11, 8) << endl;
			return r(XMM11, 8);
		} else {
			return oprnd(scnd);
		}

	} else if (is_scnd_code) {
		adjustImmediateFloat(scnd, tgt->size);

		if (needAbsCopy(scnd)) {
			os << "	movabsq " << oprnd(scnd) << ", " << r(R11, 8) << endl;
		} else {
			os << "	movq " << oprnd(scnd) << ", " << r(R11, 8) << endl;
		}
		os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
		return r(XMM11, 8);

	} else if ((is_scnd_sint || is_scnd_uint) && is_scnd_mem) {
		moveMemToReg(scnd, R11);
		os << endl;
		os << "	cvtsi2sd " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
		return r(XMM11, 8);

	} else {
		BOOST_ASSERT(false);
	}
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(tgt->alloc_type != GA_MEM || !is_scnd_mem);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str = genPreFloOperation(tgt, scnd);
		os << "	addsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	if (is_scnd_code) {
		if (scnd->data.i == 1) {
			os << "	incq " << oprnd(tgt) << "	# " << comment << endl;
			return;
		}

		if (scnd->data.i < 0) {
			PlnGenEntity e = *scnd;
			e.data.i = -e.data.i;
			e.buf == NULL;
			genSub(tgt, &e, comment);
			return;
		}
	}

	const char* add_str = oprnd(scnd);
	if (is_scnd_mem && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		os << endl;
		add_str = r(R11,8);
	}
		
	os << "	addq " << add_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || scnd->alloc_type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str = genPreFloOperation(tgt, scnd);
		os << "	subsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	if (scnd->alloc_type == GA_CODE && scnd->data.i == 1) {
		os << "	decq " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	const char* sub_str = oprnd(scnd);
	if (scnd->alloc_type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		os << endl;
		sub_str = r(R11,8);
	}

	os << "	subq " << sub_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt, string comment)
{

	if (tgt->data_type == DT_FLOAT) {
		int64_t mask = 0x8000000000000000;
		os << "	movabsq $" << mask << ", " << r(R11, 8) << endl;
		os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
		os << "	xorpd " << r(XMM11, 8) << ", " << oprnd(tgt) << "	# " << comment << endl;
	} else {
		os << "	negq " << oprnd(tgt) << "	# " << comment << endl;
	}
}

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || scnd->alloc_type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str = genPreFloOperation(tgt, scnd);
		os << "	mulsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	const char* mul_str = oprnd(scnd);
	if (scnd->alloc_type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		os << endl;
		mul_str = r(R11,8);
	}
	os << "	imulq " << mul_str << ", " << oprnd(tgt) << "	# " << comment << endl;
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type == GA_REG);
	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str = genPreFloOperation(tgt, scnd);
		os << "	divsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		return;
	}

	BOOST_ASSERT(tgt->data.i == RAX);
	const char* div_str = oprnd(scnd);
	if (scnd->alloc_type == GA_CODE) { 
		div_str = r(R11, 8);
		if (needAbsCopy(scnd)) {
			os << "	movabsq " << oprnd(scnd) << ", " << div_str << endl;
		} else {
			os << "	movq " << oprnd(scnd) << ", " << div_str << endl;
		}
	}

	if (tgt->data_type == DT_UINT && scnd->data_type == DT_UINT) {
		os << "	movq $0, %rdx" << endl;
		os << "	divq " << div_str << "	# " << comment << endl;
	} else {
		os << "	cqto"	<< endl;
		os << "	idivq " << div_str << "	# " << comment << endl;
	}
}

void PlnX86_64Generator::genCmpImmFRegMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->alloc_type == GA_CODE);
	adjustImmediateFloat(first, second->size);
	if (needAbsCopy(first)) {
		os << "	movabsq " << oprnd(first) << ", " << r(R11,8) << endl;
		os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
	} else {
		os << "	movq " << oprnd(first) << ", " << r(R11,8) << endl;
		os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
	}

	if (second->size == 4) {
		os << "	ucomiss " << oprnd(second) << ", " << r(XMM11,8) ;
	} else {
		os << "	ucomisd " << oprnd(second) << ", " << r(XMM11,8) ;
	}
}

void PlnX86_64Generator::genCmpFMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->size <= second->size);
	if (first->size < second->size) {
		BOOST_ASSERT(second->size == 8);
		os << "	cvtss2sd " << oprnd(first) << ", " << r(XMM11, 8) << endl;
	} else if (second->size == 4) {
		os << "	movss " << oprnd(first) << ", " << r(XMM11, 4) << endl;
	} else {
		BOOST_ASSERT(first->size == 8 && second->size == 8);
		os << "	movsd " << oprnd(first) << ", " << r(XMM11, 8) << endl;
	}

	if (second->size == 4) {
		os << "	ucomiss " << oprnd(second) << ", " << r(XMM11,8) ;
	} else {
		os << "	ucomisd " << oprnd(second) << ", " << r(XMM11,8) ;
	}
}

void PlnX86_64Generator::genCmpFRegFMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	BOOST_ASSERT(first->size == 8);

	if (second->size == 4) {
		os << "	cvtss2sd " << oprnd(second) << ", " << r(XMM11, 8) << endl;
		os << "	ucomisd " << r(XMM11,8) << ", " << oprnd(first);
	} else { 
		os << "	ucomisd " << oprnd(second) << ", " << oprnd(first);
	}
}

void PlnX86_64Generator::genCmpIMemFRegMem(const PlnGenEntity* first, const PlnGenEntity* second)
{
	int ireg_id;
	if (first->alloc_type == GA_MEM) {
		moveMemToReg(first, R11);
		os << endl;
		ireg_id = R11;
	} else {
		BOOST_ASSERT(first->alloc_type == GA_REG);
		ireg_id = first->data.i;;
	}

	if (second->size == 4) {
		os << "	cvtsi2ss " << r(ireg_id, 8) << ", " << r(XMM11, 4) << endl;
		os << "	ucomiss " << oprnd(second) << ", " << r(XMM11, 4);
	} else {
		os << "	cvtsi2sd " << r(ireg_id, 8) << ", " << r(XMM11, 8) << endl;
		os << "	ucomisd " << oprnd(second) << ", " << r(XMM11, 4);
	}
}

void PlnX86_64Generator::genCmpIRegMemFImm(const PlnGenEntity* first, const PlnGenEntity* second)
{
	adjustImmediateFloat(second, 8);
	int id = registerConst(second);

	if (first->size == 8 || first->alloc_type == GA_REG) {
		os << "	cvtsi2sd " << oprnd(first) << ", " << r(XMM11, 8) << endl;
	} else {
		moveMemToReg(first, R11);
		os << endl;
		os << "	cvtsi2sd " << r(R11,8) << ", " << r(XMM11, 8) << endl;
	}

	os << "	ucomisd .LD" << id << "(%rip), " << r(XMM11, 8);
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
		// 	os << "	ucomisd " << oprnd(second) << ", " << oprnd(first);

		} else {
			BOOST_ASSERT(false);
		}

		os << "	# " << comment << endl;
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
		os << "	# " << comment << endl;
		return cmp_type;
	}

	if (is_first_flo && (is_second_sint || is_second_uint)) {
		cmp_type =  genCmpI2F(second, first, rev_cmp(cmp_type));
		os << "	# " << comment << endl;
		return cmp_type;
	}

	// Integer comparison
	// cmp 2nd, 1st -  G/A:1st > 2nd, L/B:1st < 2nd
	//  cmp reg, reg
	//  cmp reg, mem  // reg(mem_min), mem
	//  cmp code, reg
	//  cmp code, mem
	if ((second->alloc_type != GA_CODE && first->alloc_type == GA_CODE) 
			|| (second->alloc_type == GA_MEM && first->alloc_type != GA_MEM)
			|| (second->alloc_type == GA_MEM && first->alloc_type == GA_MEM
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

void PlnX86_64Generator::genMemCopy(int cp_unit, string& comment)
{
	const char* safix = "";

	if (cp_unit == 8) {
		safix = "q";
	} else if (cp_unit == 4) {
		safix = "l";
	} else if (cp_unit == 2) {
		safix = "w";
	} else {
		BOOST_ASSERT(cp_unit == 1);
		safix = "b";
	}

	os << "	cld" << endl;
	os << "	rep movs" << safix << "	# " << comment << endl;
}

static string* getAdressingStr(int displacement, int base_id, int index_id, int scale)
{
	string *str = new string(r(base_id));
	string disp_s = displacement ? to_string(displacement) : "";
	if (index_id > 0) {
		string ind_s = string(r(index_id)) + "," + to_string(scale);
		*str = disp_s + "(" + *str + "," + ind_s + ")";
	} else {
		*str = disp_s + "(" + *str + ")";
	}

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
		e->type = GE_INT;
		e->alloc_type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->data.i = dp->data.intValue;

	} else if (dp->type == DP_LIT_FLO) {
		e->type = GE_FLO;
		e->alloc_type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->data.i = dp->data.intValue;

	} else if (dp->type == DP_RO_DATA) {
		if (dp->data.ro.index >= 0) {
			e->type = GE_STRING;
			e->alloc_type = GA_MEM;
			e->size = 8;
			e->data_type = DT_OBJECT_REF;
			e->data.str = new string(string("$.LC") + to_string(dp->data.ro.index));
		} else {
			int id;
			if (dp->data.ro.int_array) {
				id = registerConstArray(*(dp->data.ro.int_array), dp->data.ro.item_size);

			} else {
				BOOST_ASSERT(dp->data.ro.flo_array);
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
			}

			e->type = GE_STRING;
			e->alloc_type = GA_MEM;
			e->size = 8;
			e->data_type = DT_OBJECT_REF;
			e->data.str = new string(string("$.LD") + to_string(id));
		}

	} else
		BOOST_ASSERT(false);

	return e;
}

