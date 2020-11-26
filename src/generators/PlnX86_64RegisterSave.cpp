/// x86-64 (Linux) register save algorithm
///
/// @file	PlnX86_64RegisterSave.cpp
/// @copyright	2020 YAMAGUCHI Toshinobu 

#include <vector>
#include <algorithm>
#include <array>
#include <string>
#include <boost/assert.hpp>
#include <boost/range/algorithm_ext.hpp>
#include "../PlnModel.h"
#include "PlnX86_64DataAllocator.h"
#include "PlnX86_64Generator.h"
#include "PlnX86_64RegisterMachineImp.h"
#include "PlnX86_64RegisterSave.h"

using std::array;
using boost::remove_erase;

enum {
	REG_NO_USE = 0,
	REG_USING = 1,
	REG_SAVED = 2,
};

// Control flow graph
enum CFGStartType {
	CFGS_Entry,
	CFGS_Label,
	CFGS_Merged,
};
enum CFGEndType {
	CFGE_Next,
	CFGE_Return,
	CFGE_Jump,
	CFGE_JumpCond,
	CFGE_Merged,
};

static vector<int> save_regids = {RBX, R12, R13, R14, R15};

struct RegUsedBlock {
	int ind;
	CFGStartType start_type;
	CFGEndType end_type;

	int start_mark;
	int end_mark;

	bool deleted = false;
	
	string start_label;
	vector<string> jump_to_labels;
	string merge_info = "";

	char access_reg[REG_NUM] = {};

	vector <RegUsedBlock*> next_blocks;
	vector <RegUsedBlock*> previous_blocks;
};

enum {
	INSERT_BEFORE,
	APPEND_AFTER,
};

struct SaveRegInfo {
	int mark;
	int method;
	vector<int> regids;
};

static void addRegSaveOpe(vector<PlnOpeCode> &opecodes, vector<array<int,2>> &regmap);

#define set_regid(x)	{int id=(x); if(!access_reg[id]) access_reg[id] = REG_USING;}
inline void recUsedReg(PlnOperandInfo *ope, char access_reg[])
{
	if (!ope) return;
	if (ope->type == OP_REG) {
		set_regid(regid_of(ope));
	} else if (ope->type == OP_ADRS) {
		auto addr_ope = static_cast<PlnAdrsModeOperand*>(ope);
		set_regid(addr_ope->base_regid);
		int regid = addr_ope->index_regid;
		if (regid>0) set_regid(regid);
	}
}

void addRegSave(vector<PlnOpeCode> &opecodes, int &cur_stacksize)
{
	char access_reg[REG_NUM] = {};
	for (auto &opecode: opecodes) {
		recUsedReg(opecode.src, access_reg);
		recUsedReg(opecode.dst, access_reg);
	}

	vector<array<int,2>> regmap;
	for (int sregid: save_regids) {
		if (access_reg[sregid]) {
			cur_stacksize += 8;
			regmap.push_back({sregid, cur_stacksize});
		}
	}

	if (!regmap.size())
		return;
	
	addRegSaveOpe(opecodes, regmap);
}

static void addRegSaveOpe(vector<PlnOpeCode> &opecodes, vector<array<int,2>> &regmap)
{
	int stack_size = regmap.back()[1];

	if (stack_size % 16)
		stack_size += 8;
	
	int i=0;
	while (i < opecodes.size()) {
		PlnOpeCode &opec = opecodes[i];
		if ((opec.mne == SUBQ || opec.mne == ADDQ) && opec.dst->type == OP_REG
				&& regid_of(opec.dst) == RSP) {
			auto imm_ope = static_cast<PlnImmOperand*>(opec.src);
			imm_ope->value = stack_size;

			if (opec.mne == SUBQ) {
				for (auto &rmap: regmap) {
					auto src = new PlnRegOperand(rmap[0], 8);
					auto dst = new PlnAdrsModeOperand(RBP, -rmap[1], -1, 0);
					i++;
					BOOST_ASSERT(opecodes[i].mne == MNE_NONE); // reserved
					opecodes[i] = {MOVQ, src, dst, "save reg"};
				}

			} else if (opec.mne == ADDQ) {
				for (auto &rmap: regmap) {
					auto src = new PlnAdrsModeOperand(RBP, -rmap[1], -1, 0);
					auto dst = new PlnRegOperand(rmap[0], 8);
					i++;
					BOOST_ASSERT(opecodes[i].mne == MNE_NONE); // reserved
					opecodes[i] = {MOVQ, src, dst, "restore reg"};
				}
			}
		}
		i++;
	}
}

static RegUsedBlock* searchLabledBlock(vector<RegUsedBlock*> &blocks, string &lable);
static void mergeBlocks(vector<RegUsedBlock*> &blocks, RegUsedBlock* b1, RegUsedBlock* b2);
static void reflectUsingReg(RegUsedBlock* b, vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo, char access_reg[]);
static void addRegSaveOpeFromAnalyzedInfo(vector<PlnOpeCode> &opecodes, vector<array<int,2>> &regmap,
				vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo);
static void removeOpecodesOfBlock(vector<PlnOpeCode> &opecodes, RegUsedBlock* b);
// static void showBlockInfo(vector<RegUsedBlock*> &blocks, vector<PlnOpeCode> &opecodes);
// static void showSaveInfo(vector<PlnOpeCode> &opecodes, vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo);

void addRegSaveWithCFAnalysis(vector<PlnOpeCode> &opecodes, int &cur_stacksize)
{
	vector<RegUsedBlock*> blocks;

	RegUsedBlock* cur = NULL;
	int index = 0;
	int mark_ind = 1;
	PlnOpeCode *pre_opec = NULL;

	// create basic blocks
	for (auto &opec: opecodes) {
		opec.mark = 0;
		if (opec.mne == COMMENT || opec.mne == MNE_NONE)
			continue;

		if (opec.mne == LABEL) {
			if (cur && cur->end_mark < 0) {
				pre_opec->mark = mark_ind;
				cur->end_mark = mark_ind;
				mark_ind++;
			}

			auto new_block = new RegUsedBlock();
			new_block->ind = index++;
			new_block->start_type = CFGS_Label;
			new_block->end_type = CFGE_Next;
			new_block->start_label = string_of(opec.src);
			new_block->start_mark = mark_ind;
			new_block->end_mark = -1;

			blocks.push_back(new_block);
			cur = new_block;

			opec.mark = mark_ind;
			mark_ind++;

			pre_opec = &opec;
			continue;
		}

		if (!cur) {
			auto new_block = new RegUsedBlock();
			new_block->ind = index++;
			new_block->start_type = CFGS_Entry;
			new_block->end_type = CFGE_Next;
			new_block->start_mark = mark_ind;
			new_block->end_mark = -1;

			opec.mark = mark_ind;
			mark_ind++;
			cur = new_block;
			blocks.push_back(cur);
		}

		if (opec.mne == RET) {
			BOOST_ASSERT(cur);
			cur->end_type = CFGE_Return;
			cur->end_mark = mark_ind;
			opec.mark = mark_ind;
			mark_ind++;

			cur = NULL;

		} else if (opec.mne == JMP) {
			cur->end_type = CFGE_Jump;
			cur->jump_to_labels.push_back(string_of(opec.src));
			cur->end_mark = mark_ind;
			opec.mark = mark_ind;
			mark_ind++;
			cur = NULL;

		} else if (opec.mne == JNE || opec.mne == JE
				|| opec.mne == JGE || opec.mne == JLE
				|| opec.mne == JG || opec.mne == JL
				|| opec.mne == JAE || opec.mne == JBE
				|| opec.mne == JA || opec.mne == JB) {
			cur->end_type = CFGE_JumpCond;
			cur->jump_to_labels.push_back(string_of(opec.src));
			cur->end_mark = mark_ind;
			opec.mark = mark_ind;
			mark_ind++;
			cur = NULL;

		} else {
			recUsedReg(opec.src, cur->access_reg);
			recUsedReg(opec.dst, cur->access_reg);
		}

		pre_opec = &opec;
	}

	// build control flow graph
	for (int i=0; i<blocks.size(); i++) {
		auto b = blocks[i];
		switch (b->end_type){
			case CFGE_Next:
				if ((i+1)<blocks.size()) {
					b->next_blocks.push_back(blocks[i+1]);
					blocks[i+1]->previous_blocks.push_back(b);
				}
				break;

			case CFGE_JumpCond:
				BOOST_ASSERT((i+1)<blocks.size());
				b->next_blocks.push_back(blocks[i+1]);
				blocks[i+1]->previous_blocks.push_back(b);
			case CFGE_Jump:
				BOOST_ASSERT(b->jump_to_labels.size() == 1);
				RegUsedBlock* lblb = searchLabledBlock(blocks, b->jump_to_labels[0]);
				BOOST_ASSERT(lblb);
				b->next_blocks.push_back(lblb);
				lblb->previous_blocks.push_back(b);
				break;
		}
	}

	// showBlockInfo(blocks, opecodes);

	// Remove unreachable block
	{
		bool is_delete = true;
		while (is_delete) {
			is_delete = false;
			for (int i=1; i<blocks.size(); i++) {
				auto b = blocks[i];
				if (b->deleted) continue;
				if (b->previous_blocks.size() == 0) {	// Unreachable block
					b->deleted = true;
					for (auto nb: b->next_blocks) {
						remove_erase(nb->previous_blocks, b);
					}
					b->next_blocks.clear();
					// TODO: performance improvement
					removeOpecodesOfBlock(opecodes, b);
				}
			}
		}
	}


	// Transform graph -> tree
	for (int i=0; i<blocks.size(); i++) {
		auto b = blocks[i];
		if (b->deleted) continue;

		auto &pblocks = b->previous_blocks;
		while (pblocks.size() > 1) {
			sort(pblocks.begin(), pblocks.end(), [](const RegUsedBlock* l, const RegUsedBlock* r) { return l->ind < r->ind; });
			int pind = pblocks.back()->ind;
			if (pind > b->ind) {
				// merge with current block and last;
				mergeBlocks(blocks, b, pblocks.back());
			} else if (pind < b->ind) {
				// merge with previous 2 blocks;
				auto pb = *(pblocks.end()-2);
				mergeBlocks(blocks, pb, pblocks.back());
				if (pb->previous_blocks.size() > 1) {
					// reverse loop index to pb
					i = pb->ind - 1;
				}
			} else
				BOOST_ASSERT(false);
		}
	}

	// Merge merged blocks
	for (auto b: blocks) {
		if (b->deleted) continue;
		if (b->end_type == CFGE_Merged) {
			bool is_merged;
			do {
				is_merged = false;
				for (auto nb: b->next_blocks) {
					if (nb->start_type == CFGS_Merged) {
						mergeBlocks(blocks, b, nb);
						is_merged = true;
						break;
					}
				}
			} while(is_merged);
		}
	}

	// Create save and restore information from tree.
	vector<SaveRegInfo> saveInfo;
	vector<SaveRegInfo> restoreInfo;
	char access_reg[REG_NUM] = {};
	{
		vector<RegUsedBlock*> bstack;
		bstack.push_back(blocks[0]);
		while (bstack.size()) {
			auto b = bstack.back();
			bstack.pop_back();
			reflectUsingReg(b, saveInfo, restoreInfo, access_reg);

			for (auto nb: b->next_blocks)
				bstack.push_back(nb);
				
		}
	}

	// showBlockInfo(blocks, opecodes);
	// showSaveInfo(opecodes, saveInfo, restoreInfo);

	vector<array<int,2>> regmap;
	for (int sregid: save_regids) {
		if (access_reg[sregid]) {
			cur_stacksize += 8;
			regmap.push_back({sregid, cur_stacksize});
		}
	}

	if (regmap.size()) {
		addRegSaveOpeFromAnalyzedInfo(opecodes, regmap, saveInfo, restoreInfo);
	}
	
	for (auto b: blocks)
		delete b;
}

RegUsedBlock* searchLabledBlock(vector<RegUsedBlock*> &blocks, string &lable)
{
	for (auto b: blocks) {
		if (b->start_type == CFGS_Label && b->start_label == lable)
			return b;
	}
	BOOST_ASSERT(false);
}

static void mergeBlocksVec(vector<RegUsedBlock*> &blocks1, vector<RegUsedBlock*> &blocks2);
static void updateBlock(vector<RegUsedBlock*> &blocks, RegUsedBlock* b1, RegUsedBlock* b2);
void mergeBlocks(vector<RegUsedBlock*> &blocks, RegUsedBlock* b1, RegUsedBlock* b2)
{
	BOOST_ASSERT(b1 != b2);
	mergeBlocksVec(b1->next_blocks, b2->next_blocks);
	mergeBlocksVec(b1->previous_blocks, b2->previous_blocks);

	remove_erase(b1->next_blocks, b2);
	remove_erase(b1->next_blocks, b1);
	remove_erase(b1->previous_blocks, b2);
	remove_erase(b1->previous_blocks, b1);

	char* ra2 = b2->access_reg;
	for (auto &ra1: b1->access_reg) {
		ra1 |= *ra2;
		ra2++;
	}

	b1->start_type = CFGS_Merged;
	b1->start_mark = 0;
	b1->end_type = CFGE_Merged;
	b1->end_mark = 0;
	b1->merge_info += to_string(b2->ind) + "," + b2->merge_info;

	b2->deleted = true;

	// update other block information
	for (auto b: blocks) {
		if (b->deleted || b == b1)
			continue;
		updateBlock(b->next_blocks, b1, b2);
		updateBlock(b->previous_blocks, b1, b2);
	}
}

void reflectUsingReg(RegUsedBlock* b, vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo, char access_reg[]) {
	vector<int> regids;

	if (b->start_type != CFGS_Merged) {
		for (int i: save_regids) {
			auto &ra = b->access_reg[i];
			if ((ra & REG_USING) && !(ra & REG_SAVED)) {
				regids.push_back(i);
				ra |= REG_SAVED;
			}
		}
	}

	if (regids.size()) {
		int method = b->start_type == CFGS_Label ? APPEND_AFTER : INSERT_BEFORE;
		saveInfo.push_back({b->start_mark, method, regids});
	}

	regids.clear();

	if (b->end_type == CFGE_Return) {
		BOOST_ASSERT(b->next_blocks.size() == 0);
		for (int i: save_regids) {
			auto &ra = b->access_reg[i];
			if (ra & REG_USING) {
				BOOST_ASSERT(ra & REG_SAVED);
				regids.push_back(i);
				access_reg[i] |= ra;
			}
		}

		// insert regardress of save regnum to reset statck size.
		restoreInfo.push_back({b->end_mark, INSERT_BEFORE, regids});

		return;
	}

	for (auto nb: b->next_blocks) {
		if (nb->start_type == CFGS_Merged) {
			int i=0;
			for (int i: save_regids) {
				auto &ra = b->access_reg[i];
				auto nra = nb->access_reg[i];
				if (!(ra & REG_SAVED) && (nra & REG_USING)) {
					regids.push_back(i);
					ra |= REG_SAVED;
				}
			}
		}
	}

	if (regids.size()) {
		int method = b->end_type == CFGE_Next ? APPEND_AFTER : INSERT_BEFORE;
		saveInfo.push_back({b->end_mark, method, regids});
	}

	// refrect using reg info to next blocks.
	for (auto nb: b->next_blocks) {
		BOOST_ASSERT(!(b->end_type==CFGE_Merged && nb->start_type==CFGS_Merged));

		char* ra2 = b->access_reg;
		for (auto &ra1: nb->access_reg) {
			ra1 |= *ra2;
			ra2++;
		}
	}
}

inline int offset(vector<array<int,2>> &regmap, int regid)
{
	for (auto &rmap:regmap)
		if (rmap[0] == regid)
			return -rmap[1];
	BOOST_ASSERT(false);
}

void addRegSaveOpeFromAnalyzedInfo(vector<PlnOpeCode> &opecodes, vector<array<int,2>> &regmap,
		vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo)
{
	int stack_size = regmap.back()[1];

	if (stack_size % 16)
		stack_size += 8;
	
	BOOST_ASSERT(opecodes[0].mark == 1);
	int i;
	// special process at function begin
	for (i=1; i<10; ++i) {
		PlnOpeCode &opec = opecodes[i];
		if (opec.mne == SUBQ && regid_of(opec.dst) == RSP) {
			auto imm_ope = static_cast<PlnImmOperand*>(opec.src);
			imm_ope->value = stack_size;
			SaveRegInfo* sri = NULL;
			for (auto &svinf: saveInfo) {
				if (svinf.mark == 1) {
					sri = &svinf;
					break;
				}
			}
			if (!sri)
				break;
			for (int regid: sri->regids) {
				auto src = new PlnRegOperand(regid, 8);
				auto dst = new PlnAdrsModeOperand(RBP, offset(regmap,regid), -1, 0);
				i++;
				BOOST_ASSERT(opecodes[i].mne == MNE_NONE); // reserved
				opecodes[i] = {MOVQ, src, dst, "save reg"};
			}
			break;
		}
	}
	BOOST_ASSERT(i < 10);

	while (i < opecodes.size()) {
		PlnOpeCode &opec = opecodes[i];
		if (opec.mark) {
			for (auto &rsinf: restoreInfo) {
				if (rsinf.mark == opec.mark) {
					int j = i-7;
					BOOST_ASSERT(opecodes[i].mne == RET);
					BOOST_ASSERT(opecodes[j].mne == ADDQ && regid_of(opecodes[j].dst) == RSP);
					auto imm_ope = static_cast<PlnImmOperand*>(opecodes[j].src);
					imm_ope->value = stack_size;
					for (int regid: rsinf.regids) {
						j++;
						auto src = new PlnAdrsModeOperand(RBP, offset(regmap,regid), -1, 0);
						auto dst = new PlnRegOperand(regid, 8);
						BOOST_ASSERT(opecodes[j].mne == MNE_NONE); // reserved
						opecodes[j] = {MOVQ, src, dst, "restore reg"};
					}
					goto next_ope;
				}
			}
			for (auto &svinf: saveInfo) {
				if (svinf.mark == opec.mark) {
					vector<PlnOpeCode> savecodes;
					for (int regid: svinf.regids) {
						auto src = new PlnRegOperand(regid, 8);
						auto dst = new PlnAdrsModeOperand(RBP, offset(regmap,regid), -1, 0);
						savecodes.push_back({MOVQ, src, dst, "save reg"});
					}
					auto ins_ite = opecodes.begin() + i;
					if (svinf.method == APPEND_AFTER)
						++ins_ite;
					opecodes.insert(ins_ite, savecodes.begin(), savecodes.end());
					i += savecodes.size();
					goto next_ope;
				}
			}
		}
next_ope:
		i++;
	}
}

void mergeBlocksVec(vector<RegUsedBlock*> &blocks1, vector<RegUsedBlock*> &blocks2)
{
	blocks1.insert(blocks1.end(), blocks2.begin(), blocks2.end());
	sort(blocks1.begin(), blocks1.end(), [](const RegUsedBlock* l, const RegUsedBlock* r) { return l->ind < r->ind; });
	blocks1.erase(unique(blocks1.begin(), blocks1.end()), blocks1.end());
}

void updateBlock(vector<RegUsedBlock*> &blocks, RegUsedBlock* b1, RegUsedBlock* b2)
{
	bool replaced = false;
	auto n = blocks.begin();
	while (n != blocks.end()) {
		if (*n == b1) {
			if (replaced) {
				n = blocks.erase(n);
				continue;
			}
			replaced = true;
		} else if (*n == b2) {
			if (replaced) {
				n = blocks.erase(n);
				continue;
			}
			*n = b1;
			replaced = true;
		}
		++n;
	}
}

void removeOpecodesOfBlock(vector<PlnOpeCode> &opecodes, RegUsedBlock* b)
{
	BOOST_ASSERT(b->deleted);
	for (int i=0; i<opecodes.size(); i++) {
		if (opecodes[i].mark == b->start_mark) {
			for (;i<opecodes.size(); i++) {
				opecodes[i].mne = MNE_NONE;
				if (opecodes[i].mark == b->end_mark) {
					return;
				}
			}
			BOOST_ASSERT(false);
		}
	}

}

// void showBlockInfo(vector<RegUsedBlock*> &blocks, vector<PlnOpeCode> &opecodes)
// {
// 	// output analize info
// 	for (auto b: blocks) {
// 		string info;
// 		if (b->deleted)
// 			continue;
// 			// info = "##";
// 		info += to_string(b->ind) + ":";
// 		info += to_string(b->start_mark)+"-"+ to_string(b->end_mark)+ ":";
// 
// 		// start
// 		if (b->start_type == CFGS_Label) {
// 			info += b->start_label+ ":";
// 		} else if (b->start_type == CFGS_Merged) {
// 			info += "+:";
// 		} else {
// 			info += "-:";
// 		}
// 		// end
// 		if (b->end_type == CFGE_Return) {
// 			info += "ret:";
// 		} else if (b->end_type == CFGE_JumpCond) {
// 			info += "jpc:";
// 		} else if (b->end_type == CFGE_Jump) {
// 			info += "jmp:";
// 		} else if (b->end_type == CFGE_Merged) {
// 			info += "+:";
// 		} else {
// 			info += "-:";
// 		}
// 
// 		// next
// 		info += "[";
// 		for (auto n: b->next_blocks) {
// 			info += to_string(n->ind) + ",";
// 		}
// 		info += "]:[";
// 
// 		// previous
// 		for (auto p: b->previous_blocks) {
// 			info += to_string(p->ind) + ",";
// 		}
// 		info += "]:[";
// 		info +=	to_string(b->access_reg[RBX]);
// 		info +=	to_string(b->access_reg[R12]);
// 		info +=	to_string(b->access_reg[R13]);
// 		info +=	to_string(b->access_reg[R14]);
// 		info +=	to_string(b->access_reg[R15]);
// 
// 		info += "]:" + b->merge_info;
// 		opecodes.push_back({COMMENT, NULL, NULL, info});
// 	}
// }
// 
// void showSaveInfo(vector<PlnOpeCode> &opecodes, vector<SaveRegInfo> &saveInfo, vector<SaveRegInfo> &restoreInfo)
// {
// 	opecodes.push_back({COMMENT, NULL, NULL, "--save--"});
// 	for (auto &si: saveInfo) {
// 		string info = to_string(si.mark)+"[";
// 		for (int rid: si.regids) {
// 			switch (rid) {
// 				case RBX: info += "rbx,"; break;
// 				case R12: info += "r12,"; break;
// 				case R13: info += "r13,"; break;
// 				case R14: info += "r14,"; break;
// 				case R15: info += "r15,"; break;
// 				default: info += "*,"; break;
// 			}
// 		}
// 		info += "]";
// 		opecodes.push_back({COMMENT, NULL, NULL, info});
// 	}
// 
// 	opecodes.push_back({COMMENT, NULL, NULL, "--restore--"});
// 	for (auto &ri: restoreInfo) {
// 		string info = to_string(ri.mark);
// 		info += "[";
// 		for (int rid: ri.regids) {
// 			switch (rid) {
// 				case RBX: info += "rbx,"; break;
// 				case R12: info += "r12,"; break;
// 				case R13: info += "r13,"; break;
// 				case R14: info += "r14,"; break;
// 				case R15: info += "r15,"; break;
// 				default: info += "*,"; break;
// 			}
// 		}
// 		info += "]";
// 		opecodes.push_back({COMMENT, NULL, NULL, info});
// 	}
// } 
// 
