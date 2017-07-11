#include <iostream>
#include <string>
#include "PlnX86_64Generator.h"

using std::endl;

PlnX86_64Generator::PlnX86_64Generator(ostream& ostrm)
	: PlnGenerator(ostrm)
{
}

void PlnX86_64Generator::genSecText()
{
	os << ".text" << endl;
}

void PlnX86_64Generator::genEntryPoint(const string& entryname)
{
	os << ".global ";
	if (entryname == "main")
		os << "_start" << endl;
	else
		os << entryname << endl;
}

void PlnX86_64Generator:: genLabel(const string& label)
{
	if (label == "main") os << "_start";
	else os << label;
	
	os << ":" << endl;
}

