#include "testBase.h"

#include <iostream>
#include <fstream>
#ifdef __GNUC__
	#include <ext/stdio_sync_filebuf.h>    
	typedef __gnu_cxx::stdio_sync_filebuf<char> popen_filebuf;
#endif

#include "../PlnParser.hpp"
#include "../PlnLexer.h"
#include "../models/PlnModule.h"
#include "../generators/PlnX86_64DataAllocator.h"
#include "../generators/PlnX86_64Generator.h"

using namespace palan;

TEST_CASE ("Hello World", "[basic]")
{
	string testcode = "001_helloworld";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "Hello World!");
}

int clean()
{
	return system("rm -f out/*");
}

string build(string srcf)
{
	ifstream f;
	f.open("pacode/" + srcf + ".pa");
	if (!f) return "file open err:" + srcf;

	// parse
	PlnLexer	lexer;
	lexer.set_filename(srcf);
	lexer.switch_streams(&f, &cout);

	PlnModule module;

	PlnScopeStack	scopes;
	PlnParser parser(lexer, module, scopes);
	int ret = parser.parse();
	if (ret) return "parse err:"+srcf;

	// compile
	PlnX86_64DataAllocator allocator;
	module.finish(allocator);
	string asmf = "out/" + srcf + ".s";
	ofstream as_output;
	as_output.open(asmf, ios::out);

	PlnX86_64Generator generator(as_output);
	module.gen(generator);

	as_output.close();

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

string exec(string srcf)
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

string mcheck(string tracef)
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

