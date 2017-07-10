#include "PlnModel.h"
#include "PlnX86_64Generator.h"

int main()
{
	PlnModule modu;
	PlnFunction f1("main");
	modu.addFunc(f1);

	PlnX86_64Generator generator(std::cout);
	modu.gen(generator);

	return 0;
}
