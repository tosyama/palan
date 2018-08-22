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
	return exec_pac(srcf, "-o", srcf, "");
}

string exec_pac(string srcf, string preopt, string outf, string postopt)
{
	string log_file = "out/cui/" + (srcf != "" ? srcf : "log");

	if (outf != "") 
		outf = "out/cui/" + outf;
	if (srcf != "") 
		srcf = "pacode/" + srcf + ".pa";

	string pac_cmd = "../pac " + srcf
			+ " " + preopt + " "+ outf + " " + postopt
			+ " >" + log_file + ".out 2>" + log_file + ".err";

	int ret = getStatus(system(pac_cmd.c_str()));
	if (ret) return "compile err: "+to_string(ret);
	
	return "success";
}

string outfile(string outf)
{
	outf = "out/cui/"+outf;
	int out_fd = open(outf.c_str(), O_RDONLY);
	if (out_fd != -1) {
		string result;
		struct stat finf;
		
		if (fstat(out_fd, &finf) == 0) {
			if (finf.st_size < 10)
				result =  "too small";
			else 
				result = "exists";
		} else {
			result = "not exists";
		}
		close(out_fd);
		return result;
	}
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

