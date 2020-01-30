/// x86-64 (Linux) register machine inner class declaration.
///
/// @file	PlnX86_64RegisterMachine.h
/// @copyright	2019-2020 YAMAGUCHI Toshinobu 

class PlnOpeCode {
public:
	PlnX86_64Mnemonic mne;
	PlnOperandInfo *src, *dst;
	string comment;
	int mark;
	PlnOpeCode(PlnX86_64Mnemonic mne, PlnOperandInfo *src, PlnOperandInfo* dst, string comment)
		: mne(mne), src(src), dst(dst), comment(comment) {}
};

class PlnX86_64RegisterMachineImp
{
public:
	bool has_call = false;
	int ret_num = 0;
	int requested_stack_size = 0;
	vector<PlnOpeCode> opecodes;
	PlnX86_64RegisterMachineImp() {
	};
};
