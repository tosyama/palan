#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <stdio.h>

int main(int argc, char* argv[])
{
	int result = Catch::Session().run(argc, argv);
	
	return (result < 0xff ? result : 0xff);
}

