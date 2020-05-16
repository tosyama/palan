/// x86-64 (Linux) register save algorithm functions
///
/// @file	PlnX86_64RegisterSave.h
/// @copyright	2020 YAMAGUCHI Toshinobu 

void addRegSave(vector<PlnOpeCode> &opecodes, int cur_stacksize);
void addRegSaveWithCFAnalysis(vector<PlnOpeCode> &opecodes, int cur_stacksize);
