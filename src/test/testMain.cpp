#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <stdio.h>

#include "testBase.h"

int main(int argc, char* argv[])
{
	if (clean())
		fprintf(stderr, "Cleaning output dir is faild.");
	int result = Catch::Session().run(argc, argv);
	
	return (result < 0xff ? result : 0xff);
}

