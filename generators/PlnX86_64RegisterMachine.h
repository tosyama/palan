/// x86-64 (Linux) register machine class declaration.
///
/// @file	PlnX86_64RegisterMachine.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

enum PlnX86_64Mnemonic {
	COMMENT, LABEL,
	ADDQ, ADDSD,
	CALL,
	CLD,
	CMP, CMPB, CMPW, CMPL, CMPQ,
	CQTO,
	CVTSD2SS, CVTSI2SS, CVTSI2SD, CVTSS2SD,
	CVTTSD2SI, CVTTSS2SI,
	DECQ,
	DIVSD, DIVQ,
	IDIVQ, IMULQ,
	INCQ,
	JA, JAE, JB, JBE,
	JE,
	JG, JGE,
	JMP, JNE,
	JL, JLE,
	LEA,
	LEAVE,
	MOVABSQ,
	MOVB, MOVW, MOVL, MOVQ,
	MOVSBQ, MOVSWQ, MOVSLQ,
	MOVSS, MOVSD,
	MOVZBQ, MOVZWQ,
	MULSD,
	NEGQ,
	POPQ, PUSHQ,
	REP_MOVSQ, REP_MOVSL, REP_MOVSW, REP_MOVSB,
	RET,
	SETE, SETNE, SETL, SETG, SETLE, SETGE,
	SETB, SETA, SETBE, SETAE,
	SUBQ, SUBSD,
	SYSCALL,
	UCOMISD, UCOMISS,
	XORPD, XORQ,

	MNE_SIZE
};

enum {
	OP_REG,
	OP_IMM,
	OP_ADRS,
	OP_LBL,
	OP_LBLADRS
};

// Register operand (e.g. %rax)
class PlnRegOperand : public PlnOperandInfo {
public:
	int8_t regid;
	int8_t size;
	PlnRegOperand(int regid, int size) : PlnOperandInfo(OP_REG), regid(regid), size(size) {}
	PlnOperandInfo* clone() override { return new PlnRegOperand(regid, size); }
	const char* str(char* buf) override;
};

// Immediate operand (e.g. $10)
class PlnImmOperand : public PlnOperandInfo {
public:
	int64_t value;
	PlnImmOperand(int64_t value) : PlnOperandInfo(OP_IMM), value(value) {}
	PlnOperandInfo* clone() override { return new PlnImmOperand(value); }
	const char* str(char* buf) override;
};

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
	const char* str(char* buf) override;
};

// label operand (e.g. $.LC1)
class PlnLabelOperand : public PlnOperandInfo {
	string label;
	int id;
public:
	PlnLabelOperand(string label, int id)
		: PlnOperandInfo(OP_LBL), label(label), id(id) {}
	PlnOperandInfo* clone() override { return new PlnLabelOperand(label, id); }
	const char* str(char* buf) override;
};

// label with addressing mode operand (e.g. .LC1(%rip))
class PlnLabelAdrsModeOperand : public PlnOperandInfo {
public:
	string label;
	int8_t base_regid;
	PlnLabelAdrsModeOperand(string label, int base_regid) 
		: PlnOperandInfo(OP_LBLADRS), label(label), base_regid(base_regid) {}
	PlnOperandInfo* clone() override { BOOST_ASSERT(false); }
	const char* str(char* buf) override;
};

class PlnX86_64RegisterMachineImp;
class PlnX86_64RegisterMachine {
	PlnX86_64RegisterMachineImp *imp;
public:
	PlnX86_64RegisterMachine();
	PlnX86_64RegisterMachine(const PlnX86_64RegisterMachine&) = delete;
	void push(PlnX86_64Mnemonic mne, PlnOperandInfo *src=NULL, PlnOperandInfo* dst=NULL, string comment="");
	void addComment(string comment);
	void popOpecodes(ostream& os);
};

inline int regid_of(PlnOperandInfo *ope) {
	BOOST_ASSERT(ope->type == OP_REG);
	return static_cast<PlnRegOperand*>(ope)->regid;
}

inline int64_t int64_of(const PlnOperandInfo* ope) {
	BOOST_ASSERT(ope->type == OP_IMM);
	return static_cast<const PlnImmOperand*>(ope)->value;
}

