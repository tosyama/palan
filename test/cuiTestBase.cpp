#include <cstdlib>
#include "cuiTestBase.h"
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

TEST_CASE ("Hello World", "[basic]")
{
	string testcode = "001_helloworld";
	REQUIRE(build(testcode) == "success");
}

int clean()
{
	return system("rm -f out/cui/*");
}

inline int getStatus(int ret_status)
{
	if (WIFEXITED(ret_status)) {
		return WEXITSTATUS(ret_status);
	} else {
		return -1;
	}
}

string build(string srcf)
{
	string out_file = "out/cui/" + srcf;
	string pac_cmd = "../pac pacode/" + srcf + ".pa -o "+ out_file;

	int ret = getStatus(system(pac_cmd.c_str()));
	if (ret) return "compile err:"+srcf;
	
	int out_fd = open(out_file.c_str(), O_RDONLY);
	if (out_fd != -1) {
		struct stat finf;
		if (fstat(out_fd, &finf) == 0) {
			if (finf.st_size < 10) {
				return "too small out file";
			}
		} else {
			return "out file not exists";
		}
		close(out_fd);
	}

	return "success";
}
