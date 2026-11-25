/// Shared test helpers.
///
/// @file testBase.cpp
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include "testBase.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <future>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>

#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
		typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif
using namespace std;

// Tests name the internal tools bare, so this is the one place that knows where
// they are. Testers start in the build directory.
static const int tool_path_set = [] {
	string path = filesystem::current_path().string() + "/lib/palan/dev/tools:" + getenv("PATH");
	return setenv("PATH", path.c_str(), 1);
}();

int cleanTestEnv()
{
	system("rm -f -r out");
	return system("mkdir out");
}

string exec_worker(const string &cmd)
{
	FILE* p = popen((cmd + " 2>out/err").c_str(), "r");
	if (!p) return "exec err:" + cmd;

	popen_filebuf p_buf(p);
	istream is(&p_buf);

	string result_str;
	string tmp_str;
	getline(is, result_str);
	if (!is.eof()) result_str += "\n";
	while (getline(is, tmp_str)) {
		result_str += tmp_str;
		if (!is.eof()) result_str += "\n";
	}
	int ret = pclose(p);

	string err_str;
	struct stat st;
	if (stat("out/err", &st) == 0) {
		ifstream errfile("out/err");
		stringstream ss;
		ss << errfile.rdbuf();
		if (ss.str() != "")
			err_str = ":" + ss.str();
	}

	if (ret) {
		return "return" + to_string(WIFEXITED(ret)) + ":" + result_str + err_str;
	}
	
	return result_str + err_str;
}

string execTestCommand(const string& cmd)
{
	string result_str;

	auto f = async(launch::async, exec_worker, cmd);
	// Long enough for std in a coverage build.
	auto result = f.wait_for(chrono::seconds(20));

	if (result == future_status::timeout) {
		// time out and try killing process.
		char pid_str[10];
		string pid_cmd = "pidof -s " + cmd;
		FILE *outf = popen(pid_cmd.c_str(), "r");
		fgets(pid_str, 10, outf);
		pclose(outf);
		if (pid_str[0]) {
			pid_t pid = strtoul(pid_str, NULL, 10);
			kill(pid, SIGKILL);
		}
		return "killed by timeout(20sec):" + cmd + ":" + f.get();
	}

	return f.get();
}
