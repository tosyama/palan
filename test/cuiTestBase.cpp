#include <cstdlib>
#include "cuiTestBase.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

TEST_CASE ("Hello World", "[basic]")
{
	string testcode = "001_helloworld";
	REQUIRE(build(testcode) == "success");
	REQUIRE(outstr(testcode) == "linking: out/cui/001_helloworld\n");
	REQUIRE(errstr(testcode) == "");
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
	string pac_cmd = "../pac pacode/" + srcf + ".pa -o "+ out_file
			+ ">" + out_file + ".out 2>" + out_file + ".err";

	int ret = getStatus(system(pac_cmd.c_str()));
	if (ret) return "compile err: "+to_string(ret);
	
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

string getFileStr(string file_path)
{
	ifstream f(file_path);
	if (f.fail()) {
		return "Can't open test result: " + file_path;
	}
	stringstream sstr;
	sstr << f.rdbuf();
	return sstr.str();
}

string outstr(string srcf)
{
	string out_file = "out/cui/" + srcf + ".out";
	return getFileStr(out_file);
}

string errstr(string srcf)
{
	string out_file = "out/cui/" + srcf + ".err";
	return getFileStr(out_file);
}

