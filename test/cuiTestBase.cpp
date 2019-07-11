#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#include "cuiTestBase.h"

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

void copy_file(const string &from_file_name, const string &to_file_name)
{
	try {
		ifstream is(from_file_name, ios::in | ios::binary);
		if (is) {
			ofstream os(to_file_name, ios::out | ios::binary);

			istreambuf_iterator<char> iit(is);
			istreambuf_iterator<char> end;
			ostreambuf_iterator<char> oit(os);
			copy(iit, end, oit);
		}
	} catch(exception &e) {
		// do nothing
	}
}

string build(const string &srcf)
{
	return exec_pac(srcf, "-o", srcf, "");
}

string exec_pac(string srcf, const string &preopt, string outf, const string &postopt)
{
	string log_file = "out/cui/" + (srcf != "" ? srcf : "log");

	if (outf != "") 
		outf = "out/cui/" + outf;
	if (srcf != "") { 
		copy_file("pacode/" + srcf + ".pa", "out/cui/" + srcf + ".pa");
		srcf = "out/cui/" + srcf + ".pa";
	}

	string pac_cmd = "../pac " + srcf
			+ " " + preopt + " "+ outf + " " + postopt
			+ " >" + log_file + ".out 2>" + log_file + ".err";
	
	int ret = getStatus(system(pac_cmd.c_str()));
	if (ret) return "err: "+to_string(ret);
	
	return "success";
}

string outfile(string outf)
{
	outf = "out/cui/"+outf;
	int out_fd = open(outf.c_str(), O_RDONLY);
	string result;
	if (out_fd != -1) {
		struct stat finf;
		
		if (fstat(out_fd, &finf) == 0) {
			if (finf.st_size < 10)
				result =  "too small";
			else 
				result = "exists";
		} else {
			result = "read fail";
		}
		close(out_fd);
	} else {
		result = "not exists";
	}
	return result;
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

string outstr(const string &srcf)
{
	string out_file = "out/cui/" + srcf + ".out";
	return getFileStr(out_file);
}

string errstr(const string &srcf)
{
	string out_file = "out/cui/" + srcf + ".err";
	return getFileStr(out_file);
}

