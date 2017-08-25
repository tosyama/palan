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
#include "../generators/PlnX86_64Generator.h"

using namespace palan;

TEST_CASE ("Hello World", "[basic]")
{
	string testcode = "001_helloworld";
	REQUIRE(build(testcode) == "success");
	REQUIRE(exec(testcode) == "Hello World!");
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
	module.finish();
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

	getline(is,result_str);

	int ret = pclose(p);
	if (ret) return "return:" + to_string(ret);

	return result_str;
}

