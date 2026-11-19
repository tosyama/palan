/// semantic analyzer tests: functions and calls
///
/// @file callTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, func_defs) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/101_func_def.pa");
	ASSERT_TRUE(jout.is_object());

	auto findFunc = [&](const string& name) -> const json* {
		for (auto& f : jout["functions"])
			if (funcName(f["name"]) == name) return &f;
		return nullptr;
	};

	// func_registered_in_sa (009): add function signature
	const json* addFunc = findFunc("add");
	ASSERT_NE(addFunc, nullptr);
	ASSERT_EQ((*addFunc)["parameters"].size(), 2u);
	ASSERT_EQ((*addFunc)["parameters"][0]["name"], "a");
	ASSERT_EQ((*addFunc)["parameters"][0]["var-type"]["type-name"], "int32");
	ASSERT_EQ((*addFunc)["ret-type"]["type-name"], "int32");

	// func_params_accessible (010): identity returns its int64 param x
	const json* identFunc = findFunc("identity");
	ASSERT_NE(identFunc, nullptr);
	auto& identRet = (*identFunc)["body"][0];
	ASSERT_EQ(identRet["stmt-type"], "return");
	ASSERT_EQ(identRet["values"][0]["expr-type"], "id");
	ASSERT_EQ(identRet["values"][0]["name"], "x");
	ASSERT_EQ(identRet["values"][0]["value-type"]["type-name"], "int64");

	// func_assign_emitted (011): divmod contains assign stmt for q
	const json* divmodFunc = findFunc("divmod");
	ASSERT_NE(divmodFunc, nullptr);
	bool found_assign = false;
	for (auto& stmt : (*divmodFunc)["body"]) {
		if (stmt["stmt-type"] != "assign") continue;
		ASSERT_EQ(stmt["name"], "q");
		ASSERT_EQ(stmt["value"]["expr-type"], "id");
		ASSERT_EQ(stmt["value"]["name"], "a");
		found_assign = true;
	}
	ASSERT_TRUE(found_assign);

	// func_return_widening (012): widen returns int32 x as int64 via convert
	const json* widenFunc = findFunc("widen");
	ASSERT_NE(widenFunc, nullptr);
	auto& widenRet = (*widenFunc)["body"][0];
	ASSERT_EQ(widenRet["stmt-type"], "return");
	auto& widenVal = widenRet["values"][0];
	ASSERT_EQ(widenVal["expr-type"],               "convert");
	ASSERT_EQ(widenVal["from-type"]["type-name"],  "int32");
	ASSERT_EQ(widenVal["value-type"]["type-name"], "int64");
	ASSERT_EQ(widenVal["src"]["name"],             "x");

	// func_multiret_tapple (014): tapple-decl statement for sumsOf
	bool found_tapple = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "tapple-decl") continue;
		auto& val = stmt["value"];
		ASSERT_EQ(val["func-type"], "palan");
		ASSERT_EQ(val["value-types"].size(), 2u);
		ASSERT_EQ(stmt["vars"].size(), 2u);
		found_tapple = true;
	}
	ASSERT_TRUE(found_tapple);

	// func_recursive (015): recurse calls itself (palan call)
	const json* recurseFunc = findFunc("recurse");
	ASSERT_NE(recurseFunc, nullptr);
	bool found_recurse = false;
	for (auto& stmt : (*recurseFunc)["body"]) {
		if (stmt["stmt-type"] != "return") continue;
		auto& val = stmt["values"][0];
		ASSERT_EQ(val["expr-type"], "call");
		ASSERT_EQ(funcName(val["name"]),      "recurse");
		ASSERT_EQ(val["func-type"], "palan");
		found_recurse = true;
	}
	ASSERT_TRUE(found_recurse);
}

TEST(sa, palan_call_resolved) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/013_palan_call.pa");

	ASSERT_TRUE(jout.is_object());

	// add(1,2) is passed as an arg to printf — look inside printf's args
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& outer = stmt["body"];
		if (!outer.contains("args")) continue;
		for (auto& arg : outer["args"]) {
			if (arg["expr-type"] != "call" || funcName(arg["name"]) != "add") continue;
			ASSERT_EQ(arg["func-type"], "palan");
			ASSERT_EQ(arg["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, block_func_def) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/017_block_func_def.pa");

	// double_val is in sa["functions"]
	bool found_func = false;
	for (auto& f : jout["functions"])
		if (funcName(f["name"]) == "double_val") { found_func = true; break; }
	ASSERT_TRUE(found_func);

	// block body does NOT contain func-def (it's been processed out)
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		for (auto& bs : s["body"])
			ASSERT_NE(bs["stmt-type"], "func-def");
	}

	// block body contains the printf call as expr
	bool found_call = false;
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		for (auto& bs : s["body"])
			if (bs["stmt-type"] == "expr") { found_call = true; break; }
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, void_func) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/040_void_func.pa");

	ASSERT_TRUE(jout.is_object());

	// greet function: no ret-type, body has bare return; then the module's ..init
	ASSERT_EQ(jout["functions"].size(), 2u);
	const json& greet = jout["functions"][0];
	ASSERT_EQ(funcName(greet["name"]), "greet");
	ASSERT_FALSE(greet.contains("ret-type"));
	ASSERT_FALSE(greet.contains("rets"));
	ASSERT_EQ(greet["body"][0]["stmt-type"], "return");
	ASSERT_FALSE(greet["body"][0].contains("values"));

	// greet() called as expr stmt: no value-type on the call
	bool found_call = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || funcName(body["name"]) != "greet") continue;
		ASSERT_EQ(body["func-type"], "palan");
		ASSERT_FALSE(body.contains("value-type"));
		found_call = true;
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, lit_flo_variadic_arg)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/065_lit_flo_variadic.pa");
	ASSERT_TRUE(jout.is_object());

	// 3.14 passed to printf (variadic, no expectedType) → value-type defaults to flo64
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		const auto& call = stmt["body"];
		if (call["name"] != "printf") continue;
		for (const auto& arg : call["args"]) {
			if (arg["expr-type"] != "lit-flo") continue;
			ASSERT_EQ(arg["value-type"]["type-name"], "flo64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, named_ret_init)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/211_named_ret_init.pa");
	ASSERT_TRUE(jout.is_object());

	// Initializers run as assignments ahead of the body, in declaration order.
	const json& g = jout["functions"][0];
	ASSERT_FALSE(g["rets"][0].contains("init"));
	ASSERT_EQ(g["body"].size(), 2);
	ASSERT_EQ(g["body"][0]["name"], "x");
	ASSERT_EQ(g["body"][1]["name"], "y");
	ASSERT_EQ(g["body"][1]["value"]["expr-type"], "convert");

	const json& h = jout["functions"][1];
	ASSERT_EQ(h["body"].size(), 2);
	ASSERT_EQ(h["body"][0]["value"]["value"], "7");
	ASSERT_EQ(h["body"][1]["value"]["value"], "1");
}

TEST(sa, tapple_assign)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/212_tapple_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// Each multiple assignment becomes a block of a tapple-decl of temps plus one assignment per target.
	const json& stmts = jout["statements"];
	size_t k = 0;
	while (stmts[k]["stmt-type"] != "block") k++;
	const json& body = stmts[k]["body"];
	const json& decl = body[0];
	ASSERT_EQ(decl["stmt-type"], "tapple-decl");
	ASSERT_EQ(decl["vars"].size(), 2);
	ASSERT_EQ(decl["vars"][1]["var-type"]["type-name"], "int64");
	string t0 = decl["vars"][0]["var-name"], t1 = decl["vars"][1]["var-name"];
	ASSERT_EQ(body[1]["stmt-type"], "field-assign");
	ASSERT_EQ(body[1]["value"]["name"], t0);
	ASSERT_EQ(body[2]["stmt-type"], "assign");
	ASSERT_EQ(body[2]["name"], "i");
	ASSERT_EQ(body[2]["value"]["name"], t1);

	ASSERT_EQ(stmts[k + 1]["stmt-type"], "block");
	ASSERT_EQ(stmts[k + 1]["body"][2]["stmt-type"], "arr-assign");
}

TEST(sa, tapple_decl_convert)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/213_tapple_decl_convert.pa");
	ASSERT_TRUE(jout.is_object());

	// Only the variable whose declared type differs is received through a temp.
	const json& stmts = jout["statements"];
	size_t k = 0;
	while (stmts[k]["stmt-type"] != "tapple-decl") k++;
	const json& vars = stmts[k]["vars"];
	string t0 = vars[0]["var-name"];
	ASSERT_NE(t0, "q");
	ASSERT_EQ(vars[0]["var-type"]["type-name"], "int32");
	ASSERT_EQ(vars[1]["var-name"], "c");
	const json& conv = stmts[k + 1];
	ASSERT_EQ(conv["stmt-type"], "var-decl");
	ASSERT_EQ(conv["vars"][0]["name"], "q");
	ASSERT_EQ(conv["vars"][0]["var-type"]["type-name"], "int64");
	ASSERT_EQ(conv["vars"][0]["init"]["src"]["name"], t0);
}

TEST(sa, lit_arg)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/248_lit_arg.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 5);

	// '@[1, 2]': a temp initialized like a struct variable, passed as '@temp'.
	const auto& ro = stmts[0]["body"]["args"][0];
	ASSERT_EQ(ro["expr-type"], "temp");
	string temp = ro["body"][0]["vars"][0]["name"];
	ASSERT_EQ(ro["body"].size(), 3);
	ASSERT_EQ(ro["body"][2]["stmt-type"], "field-assign");
	ASSERT_EQ(ro["body"][2]["var"], temp);
	ASSERT_EQ(ro["value"]["name"], temp);
	ASSERT_EQ(ro["value"]["value-type"]["mutable"], false);
	ASSERT_EQ(tempRelease(ro), "free");
	ASSERT_EQ(ro["release"][0]["body"]["args"][0]["name"], temp);

	const auto& inner = stmts[1]["body"]["args"][0]["args"][0];
	ASSERT_EQ(inner["expr-type"], "temp");
	ASSERT_EQ(inner["value"]["value-type"]["mutable"], true);

	// A 'T' parameter takes the literal without '@'.
	const auto& byVal = stmts[2]["body"]["args"][0];
	ASSERT_EQ(byVal["expr-type"], "temp");
	ASSERT_FALSE(byVal["value"]["value-type"].contains("mutable"));

	ASSERT_EQ(tempRelease(stmts[3]["body"]["args"][0]), "__pln_free_Q");

	// A borrowed array is released with its sizes, as at a variable's scope end.
	const auto& arr = stmts[4]["body"]["args"][0];
	ASSERT_EQ(arr["value"]["value-type"]["mutable"], true);
	ASSERT_EQ(tempRelease(arr), "__pln_free_arr_Pint32");
	ASSERT_EQ(arr["release"][0]["body"]["args"].size(), 3);
}

// An argument whose type comes only from its parameter: a literal, or one the
// parameter's type reaches through an operator.
TEST(sa, call_arg_expected_type)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/285_call_arg_expected_type.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& lit = stmts[2]["body"]["args"][0];
	ASSERT_EQ(lit["expr-type"], "lit-flo");
	ASSERT_EQ(lit["value-type"]["type-name"], "flo64");
	const auto& shl = stmts[3]["body"]["args"][0];
	ASSERT_EQ(shl["expr-type"], "shl");
	ASSERT_EQ(shl["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[4]["body"]["args"][0]["value-type"]["type-name"], "flo32");
	const auto& add = stmts[5]["body"]["args"][0];
	ASSERT_EQ(add["right"]["expr-type"], "lit-flo");
	ASSERT_EQ(add["right"]["value-type"]["type-name"], "flo32");
	// The parameter's type reaches the shifted literal through each operator.
	ASSERT_EQ(stmts[6]["body"]["args"][0]["operand"]["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[7]["body"]["args"][0]["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[8]["body"]["args"][0]["right"]["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[9]["body"]["args"][0]["left"]["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[10]["body"]["args"][0]["right"]["left"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[11]["body"]["args"][0]["left"]["left"]["value-type"]["type-name"], "int32");
}

// An operator over a struct calls the function it names, each operand passed
// as the function's parameter takes it: borrowed, or copied into a 'T'.
TEST(sa, operator_func)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/330_operator_func.pa"));
	ASSERT_TRUE(jout.is_object());
	const string vec2 = "R4vec2";
	const auto& fs = jout["functions"];
	ASSERT_EQ(fs[0]["name"], "_330_operator_func..0add." + vec2 + "." + vec2);
	ASSERT_EQ(fs[1]["name"], "_330_operator_func..0sub.P4vec2");

	const auto& stmts = jout["statements"];
	// c = a + b: both borrowed, the struct returned into c.
	const auto& add = stmts[7]["body"];
	ASSERT_EQ(add["name"], fs[0]["name"]);
	ASSERT_EQ(add["args"][0]["value-type"]["mutable"], false);
	ASSERT_EQ(add["ret-dest"]["name"], "c");
	// d = -(a + b): the sum handed to the owning parameter without a copy.
	const auto& neg = stmts[9]["body"];
	ASSERT_EQ(neg["name"], fs[1]["name"]);
	ASSERT_EQ(neg["args"][0]["expr-type"], "temp");
	ASSERT_EQ(neg["args"][0]["body"][1]["body"]["name"], fs[0]["name"]);
	// e = a * 2.0 * b: the product borrowed by the dot product, the literal
	// typed by the parameter.
	const auto& dot = stmts[10]["vars"][0]["init"];
	ASSERT_EQ(dot["name"], fs[3]["name"]);
	const auto& scaled = dot["args"][0]["body"][1]["body"];
	ASSERT_EQ(scaled["name"], fs[2]["name"]);
	ASSERT_EQ(scaled["args"][1]["value-type"]["type-name"], "flo32");
	// f = a.x * 2.0 + b.y: no struct, plain arithmetic.
	ASSERT_EQ(stmts[11]["vars"][0]["init"]["expr-type"], "add");
}

// An operator over a struct field or a call returning a struct is no number a
// parameter types, so an overloaded function taking the struct is chosen.
TEST(sa, operator_func_overload_arg)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/331_operator_func_overload_arg.pa"));
	ASSERT_TRUE(jout.is_object());
	const string gVec2 = "_331_operator_func_overload_arg..g.P4vec2";
	vector<string> names;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr") names.push_back(s["body"]["name"]);
	ASSERT_EQ(names, vector<string>({gVec2, gVec2, "_331_operator_func_overload_arg..g.int32"}));
}

// A call whose functions return different types is told by its arguments,
// unless one is a literal.
TEST(sa, overload_inner_call)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/351_overload_inner_call.pa"));
	ASSERT_TRUE(jout.is_object());
	vector<string> names;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr") names.push_back(s["body"]["name"]);
	ASSERT_EQ(names, vector<string>({"_351_overload_inner_5fcall..g.P4vec2", "_351_overload_inner_5fcall..g.flo64"}));
}

// An omitted argument is the default, typed by its parameter, wherever the
// call is written; a string default gets a label only when a call uses it.
TEST(sa, param_default)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/339_param_default.pa"));
	ASSERT_TRUE(jout.is_object());
	for (auto& f : jout["functions"])
		for (auto& p : f["parameters"]) {
			ASSERT_FALSE(p.contains("init")) << f["name"];
			ASSERT_FALSE(p.contains("default")) << f["name"];
		}
	ASSERT_EQ(jout["str-literals"], json::parse(R"([{"label": ".str0", "value": "hi"}, {"label": ".str1", "value": "x"}])"));
	vector<json> calls;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr") calls.push_back(s["body"]);
	ASSERT_EQ(calls.size(), 5u);
	for (size_t i : {0, 3}) {
		const auto& args = calls[i]["args"];
		ASSERT_EQ(args.size(), 5u) << i;
		ASSERT_EQ(args[1]["value"], "8") << i;
		ASSERT_EQ(args[1]["value-type"]["type-name"], "int64") << i;
		ASSERT_EQ(args[2]["value"], "1") << i;
		ASSERT_TRUE(args[2]["value-type"].contains("enum")) << i;
		ASSERT_EQ(args[3]["value-type"]["type-name"], "flo64") << i;
		ASSERT_EQ(args[4]["label"], ".str0") << i;
	}
	ASSERT_EQ(calls[1]["args"][1]["value"], "5");
	ASSERT_EQ(calls[2]["args"][1]["value"], "3");
	// `b = 2` after `int32 a` takes a's type.
	ASSERT_EQ(calls[4]["args"][1]["value-type"]["type-name"], "int32");
}

// A variadic parameter is two: the array sized at run time and its length.
// A call lays its arguments out in a frame array (NULL with none) and passes
// their count; struct elements are copies the callee finalizes, not frees.
TEST(sa, variadic)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/341_variadic.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& sum = jout["functions"][0];
	ASSERT_EQ(sum["name"].get<string>().substr(sum["name"].get<string>().rfind("..")), "..sum.int32.Xint64");
	ASSERT_EQ(sum["parameters"].size(), 3u);
	ASSERT_EQ(sum["parameters"][1]["var-type"]["run-size"], true);
	ASSERT_EQ(sum["parameters"][2]["name"], "__xs_d0");
	ASSERT_EQ(sum["body"].back()["values"][0]["src"]["name"], "__xs_d0");
	const auto& fini = jout["functions"][1]["body"].back()["body"];
	ASSERT_NE(fini["name"].get<string>().find("fini"), string::npos);

	vector<json> calls;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") calls.push_back(st["body"]);
	const auto& three = calls[0]["args"];
	ASSERT_EQ(three.size(), 3u);
	ASSERT_EQ(three[1]["expr-type"], "temp");
	ASSERT_EQ(three[1]["body"][0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(three[1]["body"][0]["vars"][0]["init"]["size"], 16);
	ASSERT_EQ(three[1]["body"][2]["value"]["value-type"]["type-name"], "int64");
	ASSERT_EQ(three[2]["value"], "2");
	const auto& none = calls[1]["args"];
	ASSERT_EQ(none[1]["value"], "0");
	ASSERT_EQ(none[1]["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(none[2]["value"], "0");
	// The copy of `p` is the callee's: the caller releases nothing.
	ASSERT_TRUE(calls[2]["args"][0]["release"].empty());
}

TEST(sa, spread_arg)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/354_spread_arg.pa"));
	ASSERT_TRUE(jout.is_object());
	vector<json> calls;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") calls.push_back(st["body"]);
	auto freed = [](const json& temp) {
		vector<string> names;
		for (auto& r : temp["release"]) {
			EXPECT_EQ(r["body"]["name"], "free");
			names.push_back(r["body"]["args"][0]["name"]);
		}
		return names;
	};
	// A copy: a new array, copied into, whose block alone is freed.
	const auto& copy = calls[0]["args"];
	ASSERT_EQ(copy.size(), 2u);
	ASSERT_EQ(copy[0]["expr-type"], "temp");
	ASSERT_EQ(copy[0]["value"]["name"], "__va_0");
	ASSERT_EQ(copy[0]["body"][1]["stmt-type"], "expr");
	ASSERT_EQ(freed(copy[0]), vector<string>({"__va_0"}));
	ASSERT_FALSE(copy[0].contains("reset"));
	ASSERT_EQ(copy[1]["value"], "3");
	ASSERT_EQ(copy[1]["value-type"]["type-name"], "uint64");
	// A length known at run time is the copy's own.
	ASSERT_EQ(calls[1]["args"][0]["value"]["value-type"]["run-size"], true);
	ASSERT_EQ(calls[1]["args"][1]["name"], "____va_1_d0");
	// A given `[+]T`: its data is passed, and the data and the header are freed.
	const auto& varr = calls[2]["args"];
	ASSERT_EQ(varr[0]["value"]["name"], varr[0]["release"][0]["body"]["args"][0]["name"]);
	ASSERT_EQ(freed(varr[0]).size(), 2u);
	ASSERT_TRUE(varr[0].contains("reset"));
	ASSERT_EQ(varr[1]["src"]["offset"], 0);
	// Only the variadic one of the overloads takes a spread.
	ASSERT_NE(calls[3]["name"].get<string>().find("pick.Xint64"), string::npos);
	// A given array is passed as it is.
	const auto& given = calls[4]["args"];
	ASSERT_EQ(given[0]["reset"][0]["name"], "a");
	ASSERT_EQ(freed(given[0]), vector<string>({given[0]["value"]["name"].get<string>()}));
	// A variadic parameter is copied with its length.
	const auto& fwd = jout["functions"][1]["body"][0]["values"][0]["args"];
	ASSERT_EQ(fwd[0]["body"][0]["vars"][0]["init"]["name"], "__xs_d0");
}

// '==' and '<' are called for struct operands; the other comparisons are
// derived from them, and a comparison with NULL stays a pointer comparison.
TEST(sa, compare_func)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/337_compare_func.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& fs = jout["functions"];
	const string eq = fs[0]["name"], lt = fs[1]["name"], ltInt = fs[2]["name"];
	ASSERT_EQ(eq.rfind("_337_compare_func..0eq.", 0), 0u);
	ASSERT_EQ(lt.rfind("_337_compare_func..0lt.", 0), 0u);
	map<string, json> init;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "var-decl") init[s["vars"][0]["name"]] = s["vars"][0]["init"];

	// The function called, whether its result is negated, and the operands passed.
	auto check = [&](const string& var, const string& func, bool negated, const string& first, const string& second) {
		json call = init[var];
		ASSERT_EQ(call["expr-type"] == "logical-not", negated) << var;
		if (negated) call = call["operand"];
		ASSERT_EQ(call["name"], func) << var;
		ASSERT_EQ(call["args"][0]["name"], first) << var;
		ASSERT_EQ(call["args"][1]["name"], second) << var;
	};
	check("r1", eq, false, "a", "b");
	check("r2", eq, true, "a", "b");
	check("r3", lt, false, "a", "b");
	check("r4", lt, false, "b", "a");
	check("r5", lt, true, "b", "a");
	check("r6", lt, true, "a", "b");
	// 2 > a: the literal typed by the parameter it is passed to.
	ASSERT_EQ(init["r7"]["name"], ltInt);
	ASSERT_EQ(init["r7"]["args"][1]["value-type"]["type-name"], "int32");
	ASSERT_EQ(init["r8"]["expr-type"], "cmp");
	ASSERT_EQ(init["r9"]["expr-type"], "cmp");
	// next(@!a) > b: next() is evaluated into a temp before b.
	const auto& ahead = init["r10"];
	ASSERT_EQ(ahead["expr-type"], "temp");
	ASSERT_EQ(ahead["body"][0]["vars"][0]["init"]["name"], fs[3]["name"]);
	ASSERT_EQ(ahead["value"]["name"], lt);
	ASSERT_EQ(ahead["value"]["args"][0]["name"], "b");
	ASSERT_EQ(ahead["value"]["args"][1]["name"], ahead["body"][0]["vars"][0]["name"]);
}

// The left operand of '<<' is passed as a method-form receiver is, and a
// nested '<<' passes on the borrow its call returns.
TEST(sa, append)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/338_append.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& fs = jout["functions"];
	const string appInt = fs[0]["name"], appAcc = fs[1]["name"], get = fs[2]["name"];
	ASSERT_EQ(appInt.rfind("_338_append..0app.", 0), 0u);
	const auto& st = jout["statements"];
	size_t n = st.size();
	auto call = [&](size_t fromEnd) -> const json& { return st[n - fromEnd]["body"]; };
	// a << 1
	ASSERT_EQ(call(5)["name"], appInt);
	ASSERT_EQ(call(5)["args"][0]["name"], "a");
	ASSERT_EQ(call(5)["args"][1]["value-type"]["type-name"], "int64");
	// a << b: the overload taking '@Acc'
	ASSERT_EQ(call(4)["name"], appAcc);
	// p << 2 << 3
	ASSERT_EQ(call(3)["args"][0]["name"], appInt);
	ASSERT_EQ(call(3)["args"][0]["args"][0]["name"], "p");
	// bx.acc << 4
	ASSERT_EQ(call(2)["args"][0]["expr-type"], "field-access");
	// get(@!a) << 5
	ASSERT_EQ(call(1)["args"][0]["name"], get);
}

// Copying into an existing value calls the type's '->', or the generated
// function writing the parts of a struct holding one; initializing does not.
TEST(sa, copy_func)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/343_copy_func.pa"));
	ASSERT_TRUE(jout.is_object());
	const json& f = jout["functions"][0];
	const string copyFunc = f["name"];
	ASSERT_EQ(copyFunc.rfind("__pln_cp_", 0), 0u);
	ASSERT_TRUE(f["export"].get<bool>());
	const auto& st = jout["statements"];
	// a -> b
	ASSERT_EQ(st[2]["body"]["name"], copyFunc);
	ASSERT_EQ(st[2]["body"]["args"][0]["name"], "a");
	ASSERT_EQ(st[2]["body"]["args"][1]["name"], "b");
	// T c = a
	ASSERT_EQ(st[4]["body"]["name"], "memcpy");
	// p -> q
	ASSERT_EQ(st[7]["body"]["name"].get<string>().rfind("__allocators..__pln_assign_", 0), 0u);
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes[0]["copy-func"], copyFunc);
	ASSERT_TRUE(shapes[1]["assigns"].get<bool>());
	ASSERT_FALSE(shapes[1].contains("copy-func"));
}
