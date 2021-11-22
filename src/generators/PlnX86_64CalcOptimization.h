/// x86-64 (Linux) calculation optimization declarations.
///
/// @file	PlnX86_64CalcOptimization.h
/// @copyright	2021 YAMAGUCHI Toshinobu 

bool tryOptiMul(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment);
bool tryOptiDiv(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment);
bool tryOptiMod(PlnX86_64RegisterMachine &m, PlnGenEntity* tgt, PlnGenEntity* scnd, string& comment);
