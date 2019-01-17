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

bool nmigrate = true;

enum {
	OP_REG,
	OP_IMM,
	OP_ADRS,
	OP_LBL
};

// Register operand (e.g. %rax)
class PlnRegOperand : public PlnOperandInfo {
public:
	int8_t regid;
	int8_t size;
	PlnRegOperand(int regid, int size) : PlnOperandInfo(OP_REG), regid(regid), size(size) {}
	PlnOperandInfo* clone() override { return new PlnRegOperand(regid, size); }
	char* str(char* buf) override { return strcpy(buf, r(regid, size)); }
};
inline PlnRegOperand* reg(int regid, int size=8) { return new PlnRegOperand(regid, size); }

// Immediate operand (e.g. $10)
class PlnImmOperand : public PlnOperandInfo {
public:
	int64_t value;
	PlnImmOperand(int64_t value) : PlnOperandInfo(OP_IMM), value(value) {}
	PlnOperandInfo* clone() override { return new PlnImmOperand(value); }
	char* str(char* buf) override { sprintf(buf, "$%ld", value); return buf; }
};
inline PlnImmOperand* imm(int64_t value) { return new PlnImmOperand(value); }

// Addressing mode(Memory access) operand (e.g. -8($rax))
class PlnAdrsModeOperand : public PlnOperandInfo {
public:
	int8_t base_regid;
	int32_t displacement;
	int8_t index_regid;
	int8_t scale;
	PlnAdrsModeOperand(int base_regid, int displacement, int index_regid, int scale)
		: PlnOperandInfo(OP_ADRS),  base_regid(base_regid), displacement(displacement), index_regid(index_regid), scale(scale) {}
	PlnOperandInfo* clone() override { return new PlnAdrsModeOperand(base_regid, displacement, index_regid, scale); }
	char* str(char* buf) override {
		char *buf2 = buf;
		if (displacement != 0)
			buf2 += sprintf(buf, "%d", displacement);
		if (index_regid == -1) sprintf(buf2, "(%s)", r(base_regid,8));
		else sprintf(buf2, "(%s,%s,%d)", r(base_regid,8), r(index_regid,8), scale);

		return buf;
	}
};
inline PlnAdrsModeOperand* adrs(int base_regid, int displacement=0, int index_regid=-1, int scale=0) {
	return new PlnAdrsModeOperand(base_regid, displacement, index_regid, scale);
}

// label operand (e.g. $.LC1)
class PlnLabelOperand : public PlnOperandInfo {
	string label;
	int8_t indirect_regid;
public:
	PlnLabelOperand(string label, int indirect_regid)
		: PlnOperandInfo(OP_LBL), label(label), indirect_regid(indirect_regid) {}
	PlnOperandInfo* clone() override { return new PlnLabelOperand(label, indirect_regid); }
	char* str(char* buf) override { return strcpy(buf, label.c_str()); }
};
inline PlnLabelOperand* lbl(const string &label, int indirect_regid=-1) {
	return new PlnLabelOperand(label, indirect_regid);
}

class PlnOpeCode {
public:
	const char *mnemonic;
	PlnOperandInfo *src, *dst;
	string comment;
	PlnOpeCode(const char *mnemonic, PlnOperandInfo *src, PlnOperandInfo* dst, string comment)
		: mnemonic(mnemonic), src(src), dst(dst), comment(comment) {}
};
inline PlnOperandInfo* ope(const PlnGenEntity* e) {
	return e->ope->clone();
}

ostream& operator<<(ostream& out, const PlnOpeCode& oc)
{
	char buf1[256], buf2[256];
	if (!oc.mnemonic) {	// only comment
		if (nmigrate) out << "#";
		if (oc.src) {	// label
			BOOST_ASSERT(oc.src->type == OP_LBL);
			out << oc.src->str(buf1);
		}
		if (oc.comment != "") out << "	# " << oc.comment;
		return out;
	}

	if (nmigrate) out << "#";
	out << "	" << oc.mnemonic;
	if (oc.src) out << " " << oc.src->str(buf1);
	if (oc.dst) out << ", " << oc.dst->str(buf2);
	if (oc.comment != "") out << "	# " << oc.comment;
	return out;
}

class PlnRegisterMachine {
public:
	vector<PlnOpeCode> opecodes;

	void push(const char *mnemonic, PlnOperandInfo *src=NULL, PlnOperandInfo* dst=NULL, string comment="") {
		opecodes.push_back({mnemonic, src, dst, comment});
	}

	void addComment(string comment) {
		// BOOST_ASSERT(opecodes.back().comment == "");
		opecodes.back().comment = comment;
	}

	void popOpecodes(ostream& os) {
		for (PlnOpeCode& oc: opecodes) {
			os << oc << "\n";
		}
		os.flush();
		opecodes.clear();
	}
};

static PlnRegisterMachine m;

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
	if (nmigrate) {
		if (s == "") os << endl;
		else os << "# " << s << endl;
	}
	m.push(NULL,NULL,NULL,s);
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
		if (nmigrate) os << "_start:" << endl;
		m.push(NULL, lbl("_start:"));
		require_align = true;
	} else {
		if (nmigrate) os << label << ":" << endl;
		m.push(NULL, lbl(label+":"));
	}
}

void PlnX86_64Generator::genJumpLabel(int id, string comment)
{
	if (nmigrate) {
		os << ".L" << id << ":";
		if (comment != "")
			os << "		# " << comment;
		os << endl;
	}
	m.push(NULL, lbl(".L" + to_string(id) + ":"), NULL, comment);
}

void PlnX86_64Generator::genJump(int id, string comment)
{
	if (nmigrate) {
		os << "	jmp .L" << id;
		if (comment != "")
			os << "		# " << comment;
		os << endl;
	}
	m.push("jmp", lbl(".L" + to_string(id)), NULL, comment);
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
	if (nmigrate) {
		os << "	" << jcmd << " .L" << id;
		if (comment != "")
			os << "		# " << comment;
		os << endl;
	}
	m.push(jcmd, lbl(".L" + to_string(id)), NULL, comment);
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
	if (nmigrate) {
		os << "	" << jcmd << " .L" << id;
		if (comment != "")
			os << "		# " << comment;
		os << endl;
	}
	m.push(jcmd, lbl(".L" + to_string(id)), NULL, comment);
}

void PlnX86_64Generator::genEntryFunc()
{
	if (nmigrate) {
		os << "	pushq %rbp" << endl;
		os << "	movq %rsp, %rbp" << endl;
	}
	m.push("pushq", reg(RBP));
	m.push("movq", reg(RSP), reg(RBP));
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

	if (nmigrate) os << "	subq $" << size << ", %rsp" << endl;

	m.push("subq", imm(size), reg(RSP));
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
	if (nmigrate) os << "	movq " << r(regid) << ", " << oprnd(dst) << endl;
	m.push("movq", reg(regid), ope(dst));
}

void PlnX86_64Generator::genLoadReg(int regid, PlnGenEntity* src)
{
	if (nmigrate) os << "	movq " << oprnd(src) << ", " << r(regid) << endl;
	m.push("movq", ope(src), reg(regid));
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
			if (nmigrate) os << "	movq $" << flo_cnt << ", %rax" << endl;
			m.push("movq", imm(flo_cnt), reg(RAX));
		} else {
			if (nmigrate) os << "	xorq %rax, %rax" << endl;
			m.push("xorq", reg(RAX), reg(RAX));
		}
	}
	if (nmigrate) os << "	call " << cfuncname << endl;
	m.push("call", lbl(cfuncname));
}

void PlnX86_64Generator::genSysCall(int id, const string& comment)
{
	if (nmigrate) {
		os << "	movq $" << id << ", %rax	# " <<  comment << endl;
		os << "	syscall" << endl;
	}
	m.push("movq", imm(id), reg(RAX), comment);
	m.push("syscall");
}

void PlnX86_64Generator::genReturn()
{
	if (nmigrate) {
		os << "	leave" << endl;
		os << "	ret" << endl;
	}

	m.push("leave");
	m.push("ret");
}

void PlnX86_64Generator::genMainReturn()
{
	if (nmigrate) {
		os << "	movq %rbp, %rsp" << endl;
		os << "	popq %rbp" << endl;
		os << "	xorq %rdi, %rdi" << endl;
		os << "	movq $60, %rax" << endl;
		os << "	syscall"<< endl;
	}

	m.push("movq", reg(RBP), reg(RSP));
	m.push("popq", reg(RBP));
	m.push("xorq", reg(RDI), reg(RDI));
	m.push("movq", imm(60), reg(RAX));
	m.push("syscall");
}

void PlnX86_64Generator::moveMemToReg(const PlnGenEntity* mem, int regid)
{
	BOOST_ASSERT(mem->alloc_type == GA_MEM);
	const char* mnemonic = "";
	int regsize = 8;

	string dst_safix = "q";
	string src_safix = "";

	const char* srcstr = oprnd(mem);
	const char* dststr = r(regid, 8);

	if (mem->data_type == DT_SINT) {
		switch (mem->size) {
			case 1: src_safix = "sb"; break;
			case 2: src_safix = "sw"; break;
			case 4: src_safix = "sl"; break;
		}
		switch (mem->size) {
			case 1: mnemonic = "movsbq"; break;
			case 2: mnemonic = "movswq"; break;
			case 4: mnemonic = "movslq"; break;
			case 8: mnemonic = "movq"; break;
		}
	} else { // unsigned
		switch (mem->size) {
			case 1: src_safix = "zb"; break;
			case 2: src_safix = "zw"; break;
			case 4:
					src_safix = "";
					dststr = r(regid, 4);
					dst_safix = "l";
					break;
		}
		switch (mem->size) {
			case 1: mnemonic = "movzbq"; break;
			case 2: mnemonic = "movzwq"; break;
			case 4: mnemonic = "movl"; regsize = 4; break;
			case 8: mnemonic = "movq"; break;
		}
	}

	if (nmigrate) os << "	mov" << src_safix << dst_safix << " " << srcstr << ", " << dststr;
	m.push(mnemonic, ope(mem), reg(regid, regsize));
}

void PlnX86_64Generator::moveRegTo(int regid, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->alloc_type == GA_MEM || dst->alloc_type == GA_REG);
	const char* mnemonic = "";

	string dst_safix;
	switch (dst->size) {
		case 1: dst_safix = "b"; break;
		case 2: dst_safix = "w"; break;
		case 4: dst_safix = "l"; break;
		case 8: dst_safix = "q"; break;
		default:
			BOOST_ASSERT(false);
	}
	switch (dst->size) {
		case 1: mnemonic = "movb"; break;
		case 2: mnemonic = "movw"; break;
		case 4: mnemonic = "movl"; break;
		case 8: mnemonic = "movq"; break;
		default:
			BOOST_ASSERT(false);
	}

	if (nmigrate) os << "	mov" << dst_safix << " " << r(regid, dst->size) << ", " << oprnd(dst);
	m.push(mnemonic, reg(regid, dst->size), ope(dst));
}

static void adjustImmediateInt(const PlnGenEntity* src)
{
	BOOST_ASSERT(src->alloc_type == GA_CODE);
	if (src->buf) return;
	if (src->type == GE_FLO) {
		src->data.i = src->data.f;
		static_cast<PlnImmOperand*>(src->ope)->value = src->data.f;
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
		static_cast<PlnImmOperand*>(src->ope)->value = u.i;

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
			static_cast<PlnImmOperand*>(src->ope)->value = u.i;
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
			if (nmigrate) os << "	cvttsd2si " << oprnd(src) << ", " << r(R11, 8) << endl;
			m.push("cvttsd2si", ope(src), reg(R11));
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
			if (nmigrate) os << "	cvtss2sd " << oprnd(src) << ", " << oprnd(dst);
			m.push("cvtss2sd", ope(src), ope(dst));

		} else { // src->size == 8
			if (dst->size == 4) {
				if (nmigrate) {
					os << "	cvtsd2ss " << oprnd(src) << ", " << r(XMM11, 4) << endl;
					os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);
				}
				m.push("cvtsd2ss", ope(src), reg(XMM11,4));
				m.push("movss", reg(XMM11,4), ope(dst));

			} else { // dst->size == 8
				if (nmigrate) os << "	movsd " << oprnd(src) << ", " << oprnd(dst);
				m.push("movsd", ope(src), ope(dst));
			}
		}
		return;
	}
	
	// int/uint -> flo
	const char *src_str;
	PlnOperandInfo *src_ope;
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);
	BOOST_ASSERT(dst->size == 8);

	if (src->size == 8) {
		src_str = oprnd(src);
		src_ope = ope(src);

	} else { 
		src_str = r(R11,8);
		src_ope = reg(R11);
		moveMemToReg(src, R11);
		if (nmigrate) os << endl;
	} 

	if (nmigrate) os << "	cvtsi2sd " << src_str << ", " << oprnd(dst);
	m.push("cvtsi2sd", src_ope, ope(dst));
}

void PlnX86_64Generator::genConvFMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	if (src->size == 4 && dst->size == 8) {
		if (nmigrate) {
			os << "	cvtss2sd " << oprnd(src) << ", " << r(XMM11, 4) << endl;
			os << "	movsd	" << r(XMM11, 8) << ", " << oprnd(dst);
		}
		m.push("cvtss2sd", ope(src), reg(XMM11,4));
		m.push("movsd", reg(XMM11), ope(dst));

	} else {
		BOOST_ASSERT(src->size == 8 && dst->size == 4);
		if (nmigrate) {
			os << "	cvtsd2ss " << oprnd(src) << ", " << r(XMM11, 4) << endl;
			os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);
		}
		m.push("cvtsd2ss", ope(src), reg(XMM11,4));
		m.push("mov2ss", reg(XMM11,4), ope(dst));
	}
}

void PlnX86_64Generator::genConvIMem2FMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(dst->data_type == DT_FLOAT);
	BOOST_ASSERT(dst->size == 4 || dst->size == 8);
	BOOST_ASSERT(src->data_type == DT_SINT || src->data_type == DT_UINT);
	const char *src_str;

	PlnOperandInfo *src_ope;
	if (src->size == 8) {
		src_str = oprnd(src);
		src_ope = ope(src);
	} else {
		src_str = r(R11, 8);
		src_ope = reg(R11);
		moveMemToReg(src, R11);
		if (nmigrate) os << endl;
	}

	if (dst->size == 8) {
		if (nmigrate) {
			os << "	cvtsi2sd " << src_str << ", " << r(XMM11, 8) << endl;
			os << "	movsd	" << r(XMM11, 8) << ", " << oprnd(dst);
		}
		m.push("cvtsi2sd", src_ope, reg(XMM11));
		m.push("movsd", reg(XMM11), ope(dst));
	} else if (dst->size == 4) {
		if (nmigrate) {
			os << "	cvtsi2ss " << src_str << ", " << r(XMM11, 4) << endl;
			os << "	movss	" << r(XMM11, 4) << ", " << oprnd(dst);
		}
		m.push("cvtsi2ss", src_ope, reg(XMM11,4));
		m.push("movss", reg(XMM11,4), ope(dst));
	}
}

void PlnX86_64Generator::genConvFMem2IMem(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		if (nmigrate) os << "	cvttss2si " << oprnd(src) << ", " << r(R11, 8) << endl;
		m.push("cvttss2si", ope(src), reg(R11));
	} else {
		if (nmigrate) os << "	cvttsd2si " << oprnd(src) << ", " << r(R11, 8) << endl;
		m.push("cvttsd2si", ope(src), reg(R11));
	}

	moveRegTo(R11, dst);
}

void PlnX86_64Generator::genConvFMem2IReg(const PlnGenEntity* src, const PlnGenEntity* dst)
{
	BOOST_ASSERT(src->data_type == DT_FLOAT);
	BOOST_ASSERT(src->size == 4 || src->size == 8);
	BOOST_ASSERT(dst->data_type == DT_SINT || dst->data_type == DT_UINT);

	if (src->size == 4) {
		if (nmigrate) os << "	cvttss2si " << oprnd(src) << ", " << r(dst->data.i, 8);
		m.push("cvttss2si", ope(src), reg(dst->data.i));
	} else {
		if (nmigrate) os << "	cvttsd2si " << oprnd(src) << ", " << r(dst->data.i, 8);
		m.push("cvttsd2si", ope(src), reg(dst->data.i));
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
		const char* mnemonic;
		switch (dst->size) {
			case 1: mnemonic = "movb"; break;
			case 2: mnemonic = "movw"; break;
			case 4: mnemonic = "movl"; break;
			case 8: mnemonic = "movq"; break;
		}

		if (is_dst_sint || is_dst_uint)
			adjustImmediateInt(src);
		else if (is_dst_flo) {
			adjustImmediateFloat(src, dst->size);
		}

		if (!needAbsCopy(src)) {
			if (is_dst_reg && is_dst_flo) {
				if (nmigrate) {
					os << "	movq " << oprnd(src) << ", " << r(R11, 8) << endl;
					os << "	movq	" << r(R11, 8) << ", " << oprnd(dst);
				}
				m.push("movq", ope(src), reg(R11));
				m.push("movq", reg(R11), ope(dst));
			} else {
				if (nmigrate) os << "	mov" << dst_safix << " " << oprnd(src) << ", " << oprnd(dst);
				m.push(mnemonic, ope(src), ope(dst));
			}

		} else if (is_dst_mem || is_dst_reg && is_dst_flo) {
			if (nmigrate) {
				os << "	movabsq " << oprnd(src) << ", " << r(R11, 8);
				os << endl;
				os << "	mov" << dst_safix << " " << r(R11, dst->size) << ", " << oprnd(dst);
			}
			m.push("movabsq", ope(src), reg(R11));
			m.push(mnemonic, reg(R11, dst->size), ope(dst));

		} else {
			BOOST_ASSERT(is_dst_reg);
			if (nmigrate) os << "	movabsq " << oprnd(src) << ", " << oprnd(dst);
			m.push("movabsq", ope(src), ope(dst));
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
		if (nmigrate) os << endl;
		moveRegTo(R11, dst);

	} else {
		BOOST_ASSERT(is_src_reg && is_dst_mem);
		moveRegTo(src->data.i, dst);
	}

	if (nmigrate)  {
		if (comment != "") os << "	# " << comment;
		os << endl;
	}
	m.addComment(comment);
}

void PlnX86_64Generator::genLoadAddress(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)
{
	BOOST_ASSERT(src->alloc_type == GA_MEM);
	BOOST_ASSERT(dst->size == 8);
	int regid = R11;
	if (dst->alloc_type == GA_REG) {
		regid = dst->data.i;
	}
	if (nmigrate) os << "	lea " << oprnd(src) << ", " << r(regid, 8) << "	# " << comment << endl;
	m.push("lea", ope(src), reg(regid), comment);

	if (dst->alloc_type == GA_MEM) {
		if (nmigrate) os << "	movq " << r(regid, 8) << ", " << oprnd(dst) << endl;
		m.push("movq", reg(regid), ope(dst));
	}
}

PlnOperandInfo* PlnX86_64Generator::genPreFloOperation(PlnGenEntity* tgt, PlnGenEntity* scnd, const char** res)
{
	CREATE_CHECK_FLAG(tgt);
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(is_tgt_flo && is_tgt_reg);
	BOOST_ASSERT(tgt->size == 8);

	if (is_scnd_flo && is_scnd_mem) {
		if (scnd->size == 4) {
			if (nmigrate) os << "	cvtss2sd " << oprnd(scnd) << ", " << r(XMM11, 8) << endl;
			m.push("cvtss2sd", ope(scnd), reg(XMM11));
			*res = r(XMM11, 8);
			return reg(XMM11);
		} else {
			*res = oprnd(scnd);
			return ope(scnd);
		}

	} else if (is_scnd_code) {
		adjustImmediateFloat(scnd, tgt->size);

		if (needAbsCopy(scnd)) {
			if (nmigrate) os << "	movabsq " << oprnd(scnd) << ", " << r(R11, 8) << endl;
			m.push("movabsq", ope(scnd), reg(R11));
		} else {
			if (nmigrate) os << "	movq " << oprnd(scnd) << ", " << r(R11, 8) << endl;
			m.push("movq", ope(scnd), reg(R11));
		}
		if (nmigrate) os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
		m.push("movq", reg(R11), reg(XMM11));
		*res = r(XMM11, 8);
		return reg(XMM11);

	} else if ((is_scnd_sint || is_scnd_uint) && is_scnd_mem) {
		moveMemToReg(scnd, R11);
		if (nmigrate) {
			os << endl;
			os << "	cvtsi2sd " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
		}
		m.push("cvtsi2sd", reg(R11), reg(XMM11));
		*res = r(XMM11, 8);
		return reg(XMM11);

	} else {
		BOOST_ASSERT(false);
	}
}

void PlnX86_64Generator::genAdd(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	CREATE_CHECK_FLAG(scnd);

	BOOST_ASSERT(tgt->alloc_type != GA_MEM || !is_scnd_mem);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str;
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd, &scnd_str);
		if (nmigrate) {
			os << "	addsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		}
		m.push("addsd", scnd_ope, ope(tgt), comment);
		return;
	}

	if (is_scnd_code) {
		if (scnd->data.i == 1) {
			if (nmigrate) {
				os << "	incq " << oprnd(tgt) << "	# " << comment << endl;
			}
			m.push("incq", ope(tgt), NULL, comment);
			return;
		}

		if (scnd->data.i < 0) {
			PlnGenEntity e;
			e.type = GE_INT;
			e.alloc_type = GA_CODE;
			e.size = 8;
			e.data_type = scnd->data_type;
			e.data.i = -scnd->data.i;
			e.ope = imm(-scnd->data.i);
			genSub(tgt, &e, comment);
			return;
		}
	}

	const char* add_str = oprnd(scnd);
	PlnOperandInfo* add_ope;
	if (is_scnd_mem && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		if (nmigrate) os << endl;
		add_str = r(R11,8);
		add_ope = reg(R11);
	} else
		add_ope = ope(scnd);
	
	if (nmigrate) os << "	addq " << add_str << ", " << oprnd(tgt) << "	# " << comment << endl;
	m.push("addq", add_ope, ope(tgt), comment);
}

void PlnX86_64Generator::genSub(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || scnd->alloc_type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str;
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd, &scnd_str);
		if (nmigrate) os << "	subsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		m.push("subsd", scnd_ope, ope(tgt), comment);
		return;
	}

	if (scnd->alloc_type == GA_CODE && scnd->data.i == 1) {
		if (nmigrate) os << "	decq " << oprnd(tgt) << "	# " << comment << endl;
		m.push("decq", ope(tgt), NULL, comment);
		return;
	}

	const char* sub_str = oprnd(scnd);
	PlnOperandInfo* sub_ope;
	if (scnd->alloc_type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		if (nmigrate) os << endl;
		sub_str = r(R11,8);
		sub_ope = reg(R11);
	} else {
		sub_ope = ope(scnd);
	}

	if (nmigrate) os << "	subq " << sub_str << ", " << oprnd(tgt) << "	# " << comment << endl;
	m.push("subq", sub_ope, ope(tgt), comment);
}

void PlnX86_64Generator::genNegative(PlnGenEntity* tgt, string comment)
{

	if (tgt->data_type == DT_FLOAT) {
		int64_t mask = 0x8000000000000000;
		if (nmigrate) {
			os << "	movabsq $" << mask << ", " << r(R11, 8) << endl;
			os << "	movq " << r(R11, 8) << ", " << r(XMM11, 8) << endl;
			os << "	xorpd " << r(XMM11, 8) << ", " << oprnd(tgt) << "	# " << comment << endl;
		}
		m.push("movabsq", imm(mask), reg(R11));
		m.push("movq", reg(R11), reg(XMM11));
		m.push("xorpd", reg(XMM11), ope(tgt), comment);

	} else {
		if (nmigrate) os << "	negq " << oprnd(tgt) << "	# " << comment << endl;
		m.push("negq", ope(tgt), NULL, comment);
	}
}

void PlnX86_64Generator::genMul(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type != GA_MEM || scnd->alloc_type != GA_MEM);

	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str;
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd, &scnd_str);
		if (nmigrate) os << "	mulsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		m.push("mulsd", scnd_ope, ope(tgt), comment);
		return;
	}

	const char* mul_str = oprnd(scnd);
	PlnOperandInfo* mul_ope;
	if (scnd->alloc_type == GA_MEM && scnd->size < 8) {
		moveMemToReg(scnd, R11);
		if (nmigrate) os << endl;
		mul_str = r(R11,8);
		mul_ope = reg(R11);
	} else {
		mul_ope = ope(scnd);
	}
	if (nmigrate) os << "	imulq " << mul_str << ", " << oprnd(tgt) << "	# " << comment << endl;
	m.push("imulq", mul_ope, ope(tgt), comment);
}

void PlnX86_64Generator::genDiv(PlnGenEntity* tgt, PlnGenEntity* scnd, string comment)
{
	BOOST_ASSERT(tgt->alloc_type == GA_REG);
	if (tgt->data_type == DT_FLOAT) {
		const char* scnd_str;
		PlnOperandInfo* scnd_ope = genPreFloOperation(tgt, scnd, &scnd_str);
		if (nmigrate) os << "	divsd " << scnd_str << ", " << oprnd(tgt) << "	# " << comment << endl;
		m.push("divsd", scnd_ope, ope(tgt), comment);
		return;
	}

	BOOST_ASSERT(tgt->data.i == RAX);
	const char* div_str = oprnd(scnd);
	PlnOperandInfo* div_ope;
	if (scnd->alloc_type == GA_CODE) { 
		div_str = r(R11, 8);
		div_ope = reg(R11);
		if (needAbsCopy(scnd)) {
			if (nmigrate) os << "	movabsq " << oprnd(scnd) << ", " << div_str << endl;
			m.push("movabsq", ope(scnd), reg(R11));
		} else {
			os << "	movq " << oprnd(scnd) << ", " << div_str << endl;
			m.push("movq", ope(scnd), reg(R11));
		}
	} else {
		div_ope = ope(scnd);
	}

	if (tgt->data_type == DT_UINT && scnd->data_type == DT_UINT) {
		if (nmigrate) {
			os << "	movq $0, %rdx" << endl;
			os << "	divq " << div_str << "	# " << comment << endl;
		}
		m.push("movq", imm(0), reg(RDX));
		m.push("divq", div_ope);

	} else {
		if (nmigrate) {
			os << "	cqto"	<< endl;
			os << "	idivq " << div_str << "	# " << comment << endl;
		}
		m.push("cqto");
		m.push("idivq", div_ope);
	}
	m.addComment(comment);
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

	os << "	ucomisd .LC" << id << "(%rip), " << r(XMM11, 8);
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
	if (refs.size() == 1) {
		if (nmigrate) os << "	movq $0, " << oprnd(refs[0].get()) << endl;
		m.push("movq", imm(0), ope(refs[0].get()));

	} else if (refs.size() >= 2) {
		if (nmigrate) os << "	xorq %rax, %rax" << endl;
		m.push("xorq", reg(RAX), reg(RAX));
		
		for (auto& r: refs) {
			if (nmigrate) os << "	movq %rax, " << oprnd(r.get()) << endl;
			m.push("movq", reg(RAX), ope(r.get()));
		}
	}
}

void PlnX86_64Generator::genMemCopy(int cp_unit, string& comment)
{
	const char* safix = "";
	const char* mnemonic;

	if (cp_unit == 8) {
		safix = "q";
		mnemonic = "rep movsq";
	} else if (cp_unit == 4) {
		safix = "l";
		mnemonic = "rep movsl";
	} else if (cp_unit == 2) {
		safix = "w";
		mnemonic = "rep movsw";
	} else {
		BOOST_ASSERT(cp_unit == 1);
		safix = "b";
		mnemonic = "rep movsb";
	}

	if (nmigrate) {
		os << "	cld" << endl;
		os << "	rep movs" << safix << "	# " << comment << endl;
	}
	m.push("cld");
	m.push(mnemonic, NULL, NULL, comment);
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
		e->ope = adrs(RBP, dp->data.stack.offset);

	} else if (dp->type == DP_STK_SP) {
		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.str = new string(to_string(dp->data.stack.offset) + "(%rsp)");
		e->ope = adrs(RSP, dp->data.stack.offset);

	} else if (dp->type == DP_REG) {
		e->type = GE_INT;
		e->alloc_type = GA_REG;
		e->size = dp->size;
		e->data_type = dp->data_type;
		e->data.i = dp->data.reg.id;
		e->ope = reg(dp->data.reg.id, dp->size);

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
		e->ope = adrs(
			dp->data.indirect.base_id,
			dp->data.indirect.displacement,
			dp->data.indirect.index_id,
			dp->size);

	} else if (dp->type == DP_LIT_INT) {
		e->type = GE_INT;
		e->alloc_type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->data.i = dp->data.intValue;
		e->ope = imm(dp->data.intValue);

	} else if (dp->type == DP_LIT_FLO) {
		e->type = GE_FLO;
		e->alloc_type = GA_CODE;
		e->size = 8;
		e->data_type = dp->data_type;
		e->data.i = dp->data.intValue;
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

		e->type = GE_STRING;
		e->alloc_type = GA_MEM;
		e->size = 8;
		e->data_type = DT_OBJECT_REF;
		e->data.str = new string(string("$.LC") + to_string(id));
		e->ope = lbl("$.LC" + to_string(id));

	} else
		BOOST_ASSERT(false);

	return e;
}

