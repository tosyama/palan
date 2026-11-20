/// semantic analyzer test helpers
///
/// @file saTestUtil.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <fstream>
#include <filesystem>
#include <regex>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

// Where palan-sa looks for a module's AST: out/ stands for the work tree.
string moduleAstPath(const string& pa_file)
{
	fs::path ast = "out" + fs::weakly_canonical(pa_file).string() + ".ast.json";
	fs::create_directories(ast.parent_path());
	return ast.string();
}

void genModuleAst(const string& pa_file)
{
	execTestCommand("palan-gen-ast " + pa_file + " -o " + moduleAstPath(pa_file));
}

json run_sa(const string& pa_file)
{
	string ast_out = moduleAstPath(pa_file);
	string sa_out = "out/test.sa.json";

	string gen = execTestCommand("palan-gen-ast " + pa_file + " -o " + ast_out);
	if (gen != "") return json{};

	string sa = execTestCommand("palan-sa " + ast_out + " -o " + sa_out);
	if (sa != "") return json{};

	ifstream f(sa_out);
	if (!f.is_open()) return json{};
	return json::parse(f);
}

// sa.json with each Palan type named as written: its identity holds its
// module, which a test's file name decides.
json written(const json& sa)
{
	// A name inside a symbol carries its length in front, which shrinks with it.
	static const regex prefix("([0-9]*)__pln_L([0-9]+)");
	string in = sa.dump(), out;
	auto pos = in.cbegin();
	for (smatch m; regex_search(pos, in.cend(), m, prefix); ) {
		out.append(pos, m[0].first);
		auto end = m[0].second + stoul(m[2]) + 1;  // the module and the '_' after it
		auto seq = end;
		while (seq != in.cend() && isdigit((unsigned char)*seq)) seq++;
		if (seq != end && seq != in.cend() && *seq == '_') end = seq + 1;
		if (m[1].length() > 0)
			out += to_string(stoul(m[1]) - (end - m[0].first - m[1].length()));
		pos = end;
	}
	out.append(pos, in.cend());
	return json::parse(out);
}

// The function name a Palan link name ("module..name.types") was made from;
// a C function's name is its link name.
string funcName(const json& linkName)
{
	string s = linkName;
	size_t pos = s.rfind("..");
	size_t start = pos == string::npos ? 0 : pos + 2;
	return s.substr(start, s.find('.', start) - start);
}

// A call argument held in a temp: the call that initializes it, and the
// function that releases it.
const json& tempInit(const json& arg)
{
	return arg.at("body").at(0).at("vars").at(0).at("init");
}

string tempRelease(const json& arg)
{
	return funcName(arg.at("release").at(0).at("body").at("name"));
}

void genLibSaImport()
{
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
}

void genLibSaTypes()
{
	for (string lib : {"lib_sa_types", "lib_sa_types_mid", "lib_sa_types2"})
		genModuleAst("../test/testdata/sa/" + lib + ".pa");
}
