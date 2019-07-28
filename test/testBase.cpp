#include "testBase.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <future>
#include <csignal>
#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
	typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif

#include "../generators/PlnX86_64DataAllocator.h"
#include "../generators/PlnX86_64Generator.h"
#include "../models/PlnModule.h"
#include "../PlnModelTreeBuilder.h"
#include "../PlnMessage.h"
#include "../PlnException.h"

TEST_CASE ("Hello World", "[basic]")
{
	string testcode = "001_helloworld";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "Hello World!\n13");
}

int clean()
{
	return system("rm -rf out/*");
}

int getStatus(int ret_status)
{
	if (WIFEXITED(ret_status)) {
		return WEXITSTATUS(ret_status);
	} else {
		return -1;
	}
}

string build(const string &srcf)
{
	string ast_cmd = "../pat pacode/" + srcf + ".pa -i -o out/"+srcf+".json";
	int ret = getStatus(system(ast_cmd.c_str()));
	if (ret) return "pat exec err:" + to_string(ret) + " " + ast_cmd;

	PlnModule *module;
	{
		ifstream jf;
		jf.open("out/" + srcf + ".json");
		if (!jf)
			return "file open err:" + srcf + ".json";
		json j = json::parse(jf);
		if (j["errs"].is_array()) {
			json &err = j["errs"][0];
			PlnLoc loc(err["loc"].get<vector<int>>());
			return loc.dump() + " " + err["msg"].get<string>();
		}
		PlnModelTreeBuilder modelTreeBuilder;
		try {
			module = modelTreeBuilder.buildModule(j["ast"]);
		} catch (PlnCompileError &err) {
			return err.loc.dump() + " " + PlnMessage::getErr(err.err_code, err.arg1, err.arg2);
		}
	}

	string asmf = "out/" + srcf + ".s";

	// compile
	try {
		PlnX86_64DataAllocator allocator;
		// module->finish(allocator);
		ofstream as_output;
		as_output.open(asmf, ios::out);

		PlnX86_64Generator generator(as_output);
		module->gen(allocator, generator);

		as_output.close();

		delete module;

	} catch (PlnCompileError &err) {
		return "finish:" + err.loc.dump() + " " + PlnMessage::getErr(err.err_code, err.arg1, err.arg2);
	}

	// assemble
	string cmd = "as -o tmp.o " + asmf;
	ret = system(cmd.c_str());
	if (ret) return "assemble err:"+srcf;

	// link
	string ld_cmd = "mkdir -p out && ld --dynamic-linker /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 -o out/"+srcf+" -lc tmp.o";
	ret = system(ld_cmd.c_str());
	if (ret) return "link err:"+srcf;

	return "success";
}

string exec_worker(const string &srcf)
{
	string cmd = "out/" + srcf;
	FILE* p = popen(cmd.c_str(),"r");
	if (!p) return "exec err:" + srcf;
	
	popen_filebuf p_buf(p);
	istream is(&p_buf);
	string result_str;
	string tmp_str;
	getline(is, result_str);
	while(getline(is,tmp_str)) {
		result_str += "\n" + tmp_str;
	}

	int ret = pclose(p);
	if (ret) return "return:" + to_string(ret);

	return result_str;
}

string exec(string srcf)
{
	string result_str;
	auto f = async(launch::async, exec_worker, srcf);
	auto result = f.wait_for(chrono::seconds(1));
	if (result == future_status::timeout) {
		// time out and try killing process.
		char pid_str[10];
		string cmd = "pidof -s " + srcf;
		FILE *outf = popen(cmd.c_str(), "r");
		fgets(pid_str, 10, outf);
		pclose(outf);
		if (pid_str[0]) {
			pid_t pid = strtoul(pid_str, NULL, 10);
			kill(pid, SIGKILL);
		}
		return "killed by timeout(1sec):" + srcf;
	}
	return f.get();
}

string mcheck(const string &tracef)
{
	ifstream f;
	f.open("out/" + tracef);
	if (!f) return "file open err:" + tracef;
	int alloc_cnt= 0;
	int free_cnt= 0;

	while (!f.eof()) {
		int c = f.get();
		if (c=='+') alloc_cnt++;
		else if (c=='-') free_cnt++;
	}
	return "+" + to_string(alloc_cnt) + " -" + to_string(free_cnt);
}

