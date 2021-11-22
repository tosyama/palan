/// x86-64 (Linux) calculation optimization definitions.
///
/// @file	PlnX86_64CalcOptimization.h
/// @copyright	2021 YAMAGUCHI Toshinobu 

#include <vector>
#include <string>
#include <sstream>
#include <boost/assert.hpp>

#include "../PlnModel.h"
#include "../PlnConstants.h"
#include "PlnX86_64DataAllocator.h"
#include "PlnX86_64Generator.h"

inline int log2u(unsigned int x)
{
	int r = -1;
	while (x) {
		++r;
		x /= 2;
	}
	return r;
}

bool tryOptiMul(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment)
{
	if (scnd->type != GA_CODE)
		return false;

	if (tgt->data_type == DT_FLOAT)
		return false;

	if (scnd->data_type == DT_UINT || scnd->data_type == DT_SINT) {
		int64_t n = int64_of(scnd->ope);
		int64_t mask = n-1;
		if (n>0 && !(n&mask)) {	// Check if n == 2^x
			BOOST_ASSERT(tgt->type == GA_REG);
			int shift_num = log2u(n);
			m.push(SALQ, imm(shift_num), ope(tgt), comment);
			return true;

		}
	}
	return false;
}

bool tryOptiDiv(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment)
{
	if (scnd->type != GA_CODE)
		return false;

	if (tgt->data_type == DT_FLOAT)
		return false;

	if (scnd->data_type == DT_UINT || scnd->data_type == DT_SINT) {
		int64_t n = int64_of(scnd->ope);
		int64_t mask = n-1;
		if (n>0 && !(n&mask)) {	// Check if n == 2^x
			if (tgt->data_type == DT_UINT) {
				BOOST_ASSERT(tgt->type == GA_REG);
				int shift_num = log2u(n);
				m.push(SHRQ, imm(shift_num), ope(tgt), comment);
				return true;
			} else {
				BOOST_ASSERT(tgt->data_type == DT_SINT);
				BOOST_ASSERT(tgt->size == 8);
				int shift_num = log2u(n);
				m.push(MOVQ, ope(tgt), reg(RDX));
				m.push(SHRQ, imm(64-shift_num), ope(tgt));
				m.push(ADDQ, reg(RDX), ope(tgt));
				m.push(SARQ, imm(shift_num), ope(tgt));
				return true;
			}
		}
	}
	return false;
}


bool tryOptiMod(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment)
{
	if (scnd->type != GA_CODE)
		return false;
	
	if (scnd->data_type == DT_UINT || scnd->data_type == DT_SINT) {
		int64_t n = int64_of(scnd->ope);
		int64_t mask = n-1;
		if (n>0 && !(n&mask)) {	// Check if n == 2^x
			BOOST_ASSERT(tgt->type == GA_REG);
			if (tgt->data_type == DT_UINT) {
				m.push(ANDQ, imm(mask), ope(tgt), comment);
				return true;

			} else {
				BOOST_ASSERT(tgt->data_type == DT_SINT);
				BOOST_ASSERT(tgt->size == 8);
				int shift_num = 64 - log2u(n);
				m.push(CLTQ);
				m.push(SHRQ, imm(shift_num), reg(RDX));
				m.push(ADDQ, reg(RDX), ope(tgt));
				m.push(ANDQ, imm(mask), ope(tgt), comment);
				m.push(SUBQ, reg(RDX), ope(tgt));

				return true;
			}
		}
	}
	
	return false;
}
