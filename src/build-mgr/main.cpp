/// Palan Build manager
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <getopt.h>
#include <unistd.h>
#include <filesystem>
#include <cstdint>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnBuildMgrMessage.h"
#include "../common/PlnDefs.h"
#include "../common/PlnInterface.h"
#include "../common/PlnProcess.h"
#include "PlnAllocatorGen.h"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

static string getWorkDirPath(const string& target);

// Run a pipeline tool and map the outcome onto build-mgr's historical return
// convention: 0 on success, the child's exit status when it failed, -1 when
// it died from a signal (main returns that as 255, as before). A child that
// could not be started at all was previously indistinguishable from either --
// the shell reported "command not found" on its own stderr and exited 127,
// which build-mgr forwarded blindly. Now it is diagnosed and 127 is kept as
// the returned exit code for compatibility.
static int runTool(const vector<string>& argv)
{
	PlnProcResult r = runProcess(argv);
	switch (r.status) {	// LCOV_EXCL_BR_LINE -- the Signaled arm below is unreachable under test
		case PlnSpawnStatus::Exited:
			return r.exit_code;
		// LCOV_EXCL_START
		case PlnSpawnStatus::Signaled:
			// Only reachable if one of the pipeline tools itself (gen-ast/sa/
			// codegen/as/ld) dies by signal -- not reproducible by any Palan
			// source under test; the compiled *user* program crashing is a
			// separate, tested path (see the run-and-delete step below).
			return -1;
		// LCOV_EXCL_STOP
		default:
			cerr << PlnBuildMgrMessage::getMessage(
				E_FailedToExecute, argv[0], strerror(r.err_no)) << endl;
			return 127;
	}
}

static string tempNameOf(const string& path)
{
	return path + ".tmp." + to_string(getpid());
}

// Builds racing over one work file must only ever see it complete, so it takes
// its name once finished. Every tool the pipeline runs takes `-o`.
static int runToolTo(vector<string> argv, const string& path)
{
	string tmp = tempNameOf(path);
	argv.push_back("-o");
	argv.push_back(tmp);
	if (int ret = runTool(argv)) {
		error_code ignored;
		fs::remove(tmp, ignored);
		return ret;
	}
	fs::rename(tmp, path);
	return 0;
}

static fs::file_time_type newestToolTime(const string& exec_path, const string& target)
{
	fs::file_time_type newest = fs::file_time_type::min();
	for (const string& tool : {
			exec_path + "/palan-gen-ast", exec_path + "/palan-c2ast", exec_path + "/palan-sa",
			exec_path + "/palan-codegen", exec_path + "/../" + target + "/c2ast/predefined.h"}) {
		error_code ignored;
		newest = max(newest, fs::last_write_time(tool, ignored));
	} // LCOV_EXCL_BR_LINE -- closing brace of a loop over local strings; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals
	return newest;
}

// An equal time counts as older, so an edit right after a build is not missed.
// An unreadable dependency counts as newer, for the tool to report it.
static bool isUpToDate(const string& result, fs::file_time_type tools_time, const vector<string>& deps)
{
	error_code ec;
	fs::file_time_type result_time = fs::last_write_time(result, ec);
	if (ec || result_time <= tools_time) return false;
	for (const string& dep : deps) {
		fs::file_time_type dep_time = fs::last_write_time(dep, ec);
		if (ec || result_time <= dep_time) return false;
	}
	return true;
}

// Leaves `ast` holding the AST: the headers it read are known only from the AST itself.
static int generateAst(const string& source, const string& ast_path, const string& exec_path,
                       const string& target, fs::file_time_type tools_time, json& ast)
{
	ifstream existing(ast_path);
	ast = json::parse(existing, nullptr, false);
	if (ast.is_object()) {
		vector<string> deps{source};
		for (auto& header : ast.value("c-headers", json::array())) deps.push_back(header);
		if (isUpToDate(ast_path, tools_time, deps)) return 0;
	}
	if (int ret = runToolTo({exec_path + "/palan-gen-ast", source, "--target", target}, ast_path)) // LCOV_EXCL_BR_LINE
		return ret;
	ifstream generated(ast_path);
	ast = json::parse(generated);
	return 0;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with local strings/paths; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

// `source` is the source's path in the working directory; deps are the ASTs the result comes from.
static int compileModule(const string& source, const string& mode, const string& exec_path, fs::file_time_type tools_time,
                         const vector<string>& deps, const vector<string>& library_args = {})
{
	string base = source + "." + mode;
	string sa_file = base + ".sa.json";
	string asm_file = base + ".s";
	string obj_file = base + ".o";
	if (fs::exists(sa_file) && isUpToDate(obj_file, tools_time, deps))
		return 0;
	vector<string> sa_argv{exec_path + "/palan-sa", source + ".ast.json"};
	if (mode == "release") sa_argv.push_back("--release");
	sa_argv.insert(sa_argv.end(), library_args.begin(), library_args.end());
	if (int ret = runToolTo(sa_argv, sa_file)) // LCOV_EXCL_BR_LINE
		return ret;
	if (int ret = runToolTo({exec_path + "/palan-codegen", sa_file}, asm_file)) // LCOV_EXCL_BR_LINE
		return ret;
	return runToolTo({"as", asm_file}, obj_file); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE -- closing brace of a function with local strings/paths; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

// Kept as it was when the content is the same, so its time still says what it was compiled from.
static void renameIfChanged(const string& tmp, const string& path)
{
	auto contentOf = [](const string& p) {
		ifstream f(p);
		return string(istreambuf_iterator<char>(f), {});
	};
	if (fs::exists(path) && contentOf(tmp) == contentOf(path))
		fs::remove(tmp);
	else
		fs::rename(tmp, path);
}

// Reported here since `ld` would blame the symbols, and palan-sa takes a
// module's identity as unique.
static bool claimModuleId(map<string, string>& module_ids, const string& id, const string& source)
{
	auto [it, is_new] = module_ids.emplace(id, source);
	if (!is_new)
		cerr << PlnBuildMgrMessage::getMessage(E_DuplicateModuleId, source, id, it->second) << endl;
	return is_new;
}

// `library`: the identity of the module they belong to, or empty for a program's.
// With no `link` clause, their sa.json has nothing to add to the program's libraries.
static int buildAllocators(const PlnAllocShapes& shapes, const string& dir, const string& library,
                           const string& mode, const string& exec_path, const string& target, fs::file_time_type tools_time,
                           map<string, string>& module_ids, string& obj_file)
{
	fs::create_directories(dir);
	string alloc_pa = dir + "/" PALAN_ALLOCATORS_MODULE ".pa";
	if (!claimModuleId(module_ids, allocatorsModuleOf(library), alloc_pa))
		return 1;
	string alloc_h = dir + "/" PALAN_ALLOCATORS_MODULE ".h";
	string alloc_ast = alloc_pa + ".ast.json";
	{
		ofstream hdr(tempNameOf(alloc_h));
		writeAllocatorsHeader(hdr, shapes);
		ofstream out(tempNameOf(alloc_pa));
		writeAllocatorsSource(out, shapes, library, mode == "debug");
	}
	renameIfChanged(tempNameOf(alloc_h), alloc_h);
	renameIfChanged(tempNameOf(alloc_pa), alloc_pa);

	json alloc_ast_json;
	if (int ret = generateAst(alloc_pa, alloc_ast, exec_path, target, tools_time, alloc_ast_json)) // LCOV_EXCL_BR_LINE
		return ret;
	if (int ret = compileModule(alloc_pa, mode, exec_path, tools_time, {alloc_ast})) // LCOV_EXCL_BR_LINE
		return ret;
	obj_file = alloc_pa + "." + mode + ".o";
	return 0;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with local strings/paths/json; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

// The `.pai` of an `import <module>`, or empty after reporting why there is none.
static string findLibrary(const string& module, const set<string>& libpaths, const string& std_root, const string& importer)
{
	string package = module.substr(0, module.find('/'));
	vector<string> roots;
	if (package == "std") {
		roots.push_back(std_root);
	} else {
		for (auto& dir : libpaths)
			if (fs::is_directory(dir + "/" + package)) roots.push_back(dir);
		if (roots.size() > 1) {
			cerr << PlnBuildMgrMessage::getMessage(E_LibraryInTwoPlaces, package, roots[0], roots[1]) << endl;
			return "";
		}
	}
	string pai = roots.empty() ? "" : fs::weakly_canonical(roots[0] + "/" + module + ".pai").string();
	if (pai.empty() || !fs::exists(pai)) {
		cerr << PlnBuildMgrMessage::getMessage(E_LibraryNotFound, importer, module) << endl;
		return "";
	}
	if (!fs::exists(fs::path(pai).replace_extension(".a"))) {
		cerr << PlnBuildMgrMessage::getMessage(E_LibraryNoArchive, pai) << endl;
		return "";
	}
	return pai;
}

static bool checkLibraryModules(const vector<json>& asts)
{
	auto packageOf = [](const string& id) { return id.substr(0, id.find('.')); };
	string main_id = asts[0]["module"];
	for (auto& ast : asts) {
		string id = ast["module"], source = ast["original"];
		if (!isLibraryModule(id)) {
			cerr << PlnBuildMgrMessage::getMessage(E_LibModuleNotDeclared, source) << endl;
			return false;
		}
		if (packageOf(id) != packageOf(main_id)) {
			cerr << PlnBuildMgrMessage::getMessage(E_LibPackageMismatch, source, packageOf(id), packageOf(main_id)) << endl;
			return false;
		}
	}
	return true;
}

// Writes under a temporary name in the output's directory, so an output only ever appears complete.
static void installFile(const string& from, const string& to)
{
	fs::copy_file(from, tempNameOf(to), fs::copy_options::overwrite_existing);
	fs::rename(tempNameOf(to), to);
}

// base: the main source's path in the working directory, with the mode.
static int buildLibrary(const vector<json>& asts, const string& base, const vector<string>& obj_files,
                        vector<pair<string, string>> members, const set<string>& link_objs,
                        const string& out_dir, const string& exec_path, const string& target,
                        const vector<string>& library_args)
{
	string main_id = asts[0]["module"];
	fs::path source = asts[0]["original"].get<string>();
	string lib_dir = base + ".lib";
	fs::create_directories(lib_dir);

	vector<json> internals(asts.begin() + 1, asts.end());
	string pai = lib_dir + "/" + source.stem().string() + ".pai";
	try {
		string text = writeLibraryInterface(asts[0], internals);
		ofstream(pai) << text;
	} catch (const runtime_error& e) {
		cerr << e.what() << endl;
		return 1;
	}

	// The interface's reader takes the identity from its file name, as palan-gen-ast does for the source.
	string pai_ast = pai + ".ast.json";
	string symbols_file = pai + ".symbols.json";
	if (int ret = runToolTo({exec_path + "/palan-gen-ast", pai, "--target", target}, pai_ast)) // LCOV_EXCL_BR_LINE
		return ret;
	vector<string> sa_argv{exec_path + "/palan-sa", pai_ast, "--interface"};
	sa_argv.insert(sa_argv.end(), library_args.begin(), library_args.end());
	if (int ret = runToolTo(sa_argv, symbols_file))
		return ret;
	ifstream symbols_in(symbols_file), sa_in(base + ".sa.json");
	json symbols = json::parse(symbols_in), sa = json::parse(sa_in);
	set<string> defined;
	for (auto& f : sa["functions"]) defined.insert(f["name"].get<string>());
	for (auto& g : sa.value("globals", json::array()))
		if (g.value("export", false)) defined.insert(g["label"].get<string>());
	for (auto& symbol : symbols)
		if (!defined.count(symbol)) {
			cerr << PlnBuildMgrMessage::getMessage(E_LibSymbolMismatch, source.string(), symbol) << endl;
			return 1;
		}

	// `ar` keeps only base names, which modules may share.
	vector<string> ar_argv{"ar", "rcsD"};
	string archive_tmp;
	{
		string package_dir = main_id.substr(0, main_id.rfind('.'));
		replace(package_dir.begin(), package_dir.end(), '.', '/');
		fs::path out_base = fs::path(out_dir) / package_dir / source.stem();
		fs::create_directories(out_base.parent_path());
		string archive = out_base.string() + ".a";
		archive_tmp = tempNameOf(archive);
		ar_argv.push_back(archive_tmp);
		for (int i = 0; i < asts.size(); i++)
			members.push_back({asts[i]["module"], obj_files[i]});
		int obj_no = 0;
		for (auto& obj : link_objs)
			members.push_back({"__obj" + to_string(obj_no++) + "_" + fs::path(obj).stem().string(), obj});
		for (auto& [name, obj] : members) {
			string member = lib_dir + "/" + name + ".o";
			fs::copy_file(obj, member, fs::copy_options::overwrite_existing);
			ar_argv.push_back(member);
		}
		if (int ret = runTool(ar_argv)) { // LCOV_EXCL_BR_LINE
			error_code ignored;
			fs::remove(archive_tmp, ignored);
			return ret;
		}
		fs::rename(archive_tmp, archive);
		installFile(pai, out_base.string() + ".pai");
	} // LCOV_EXCL_BR_LINE -- closing brace of a block with local strings/paths; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals
	return 0;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with many local strings/paths/json; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'v' },
		{ "clean",   no_argument,       NULL, 'c' },
		{ "output",  required_argument, NULL, 'o' },
		{ "target",  required_argument, NULL, 't' },
		{ "lib",     no_argument,       NULL, 'l' },
		{ "debug",   no_argument,       NULL, 'd' },
		{ "release", no_argument,       NULL, 'r' },
		{ 0 }
	};

	int opt, option_index = 0;
	bool do_clean = false;
	bool do_lib = false;
	string binary_name;
	bool output_specified = false;
	string target = PALAN_TARGET;
	optional<bool> release;

	while (0 < (opt = getopt_long(argc, argv, "+hvco:", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnBuildMgrMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnBuildMgrMessage::getMessage(M_Version) << endl;
				return 0;
			case 'c':
				do_clean = true;
				break;
			case 'o':
				binary_name = optarg;
				output_specified = true;
				break;
			case 't':
				target = optarg;
				break;
			case 'l':
				do_lib = true;
				break;
			case 'd':
				release = false;
				break;
			case 'r':
				release = true;
				break;
			default:
				break;
		}
	}

	if (!isSupportedTarget(target)) {
		cerr << PlnBuildMgrMessage::getMessage(E_UnsupportedTarget, target) << endl;
		return 1;
	}

	string palan_work_path = getWorkDirPath(target);
	if (palan_work_path.empty())
		return 1;

	if (do_clean) {
		fs::remove_all(palan_work_path);
		return 0;
	}

	error_code ec;
	fs::create_directories(palan_work_path, ec);
	if (ec) {
		cerr << PlnBuildMgrMessage::getMessage(E_CouldNotCreateDir, palan_work_path, ec.message()) << endl;
		return 1;
	}

	if ((argc-optind) < 1 || (output_specified && (argc-optind) != 1)) {
		cerr << PlnBuildMgrMessage::getMessage(E_WrongNumberOfArgs) << endl;
		return 1;
	}
	if (do_lib && !output_specified) {
		cerr << PlnBuildMgrMessage::getMessage(E_LibNeedsOutput) << endl;
		return 1;
	}

	const string mode = release.value_or(output_specified) ? "release" : "debug";

	fs::path exec_file_path = fs::canonical("/proc/self/exe");
	string exec_path = exec_file_path.parent_path().string();

	vector<string> target_files;
	vector<string> ast_files;
	vector<json> asts;
	// The archive members besides each module's own object: name, object file.
	vector<pair<string, string>> lib_members;
	map<string, string> module_ids;
	map<string, vector<string>> module_imports;
	set<string> libpaths;
	vector<pair<string, string>> library_imports; // importing file, module
	fs::file_time_type tools_time = newestToolTime(exec_path, target);
	target_files.push_back(fs::weakly_canonical(argv[optind]).string());
	string entry_dir = palan_work_path + target_files[0] + "." + mode + ".d";
	vector<string> run_argv;
	if (!output_specified) {
		binary_name = entry_dir + "/out";
		run_argv.push_back(binary_name);
		run_argv.insert(run_argv.end(), argv + optind + 1, argv + argc);
	}

	// Compile target files.
	for (int i=0; i<target_files.size(); i++) {
		fs::path input_file = target_files[i];
		if (!fs::exists(input_file)) {
			cerr << PlnBuildMgrMessage::getMessage(E_CouldNotOpenFile, input_file.string()) << endl;
			return 1;
		}

		string input_file_work_path = palan_work_path + input_file.parent_path().string();
		fs::create_directories(input_file_work_path);
		string output_ast_path = palan_work_path + input_file.string() + ".ast.json";

		json ast;
		if (int ret = generateAst(input_file.string(), output_ast_path, exec_path, target, tools_time, ast))
			return ret;
		ast_files.emplace_back(output_ast_path);
		asts.push_back(ast);
		for (auto& dir : ast.value("libpaths", json::array())) libpaths.insert(dir.get<string>());

		// check import files and add targets
		if (!claimModuleId(module_ids, ast["module"], input_file.string()))
			return 1;
		if (!ast["import"].is_null()) {
			for (auto imp: ast["import"]) {
				if (imp["path-type"] == "src") {
					string import_path = imp["path"];

					if (import_path.ends_with(".pa")) {
						module_imports[input_file.string()].push_back(import_path);
						auto it = find(target_files.begin(), target_files.end(), import_path);
						if (it == target_files.end()) {
							target_files.emplace_back(import_path);
						}
					}
				} else {
					library_imports.push_back({input_file.string(), imp["path"]});
				}
			}
		}
	}

	if (do_lib && !checkLibraryModules(asts))
		return 1;

	// Union of libraries requested by cinclude `link` clauses across every
	// module, and by the `link` statements of the libraries used. Declared here
	// (not scoped to the aggregation block below) so it is still alive when the
	// ld command line is built.
	set<string> link_libs;
	set<string> link_objs;

	// Only here are all libpaths known: a library's sources are not read, and its
	// `.pai` holds none. This loop grows the list by what each `.pai` imports.
	map<string, string> library_files; // module, .pai
	vector<string> library_args;
	vector<string> archives;
	string std_root = fs::weakly_canonical(exec_path + "/../" + target).string();
	for (int i=0; i<library_imports.size(); i++) {
		auto [importer, module] = library_imports[i];
		auto known = library_files.find(module);
		if (known == library_files.end()) {
			string pai = findLibrary(module, libpaths, std_root, importer);
			if (pai.empty())
				return 1;
			string pai_ast = palan_work_path + pai + ".ast.json";
			fs::create_directories(fs::path(pai_ast).parent_path());
			json ast;
			if (int ret = generateAst(pai, pai_ast, exec_path, target, tools_time, ast))
				return ret;
			if (!claimModuleId(module_ids, ast["module"], pai))
				return 1;
			for (auto& lib : ast.value("libs", json::array())) link_libs.insert(lib.get<string>());
			for (auto& imp : ast.value("import", json::array()))
				if (imp["path-type"] == "inc") library_imports.push_back({pai, imp["path"]});
			library_args.push_back("--library=" + module + "=" + pai_ast);
			archives.push_back(fs::path(pai).replace_extension(".a").string());
			known = library_files.emplace(module, pai).first;
		}
		module_imports[importer].push_back(known->second);
	}

	vector<string> obj_files;

	for (int i=0; i<ast_files.size(); i++) {
		string ast_file = ast_files[i];
		string base = ast_file.substr(0, ast_file.size() - 9);  // remove ".ast.json"

		// palan-sa reads through an imported module's own imports, so all of them count.
		vector<string> deps{ast_file};
		set<string> reached;
		function<void(const string&)> reach = [&](const string& source) {
			for (auto& import_path : module_imports[source])
				if (reached.insert(import_path).second) {
					deps.push_back(palan_work_path + import_path + ".ast.json");
					reach(import_path);
				}
		};
		reach(target_files[i]);

		if (int ret = compileModule(base, mode, exec_path, tools_time, deps, library_args))
			return ret;

		obj_files.push_back(base + "." + mode + ".o");
	}

	// Whole-program information, and the allocators: a library module's come from
	// its own shapes, the others' from the shapes of all of them.
	PlnAllocShapes program_shapes;
	for (int i=0; i<ast_files.size(); i++) {
		ifstream f(ast_files[i].substr(0, ast_files[i].size() - 9) + "." + mode + ".sa.json");
		json sa = json::parse(f);
		// SA always emits "libs" (possibly empty), so no contains() guard is needed.
		for (auto& l : sa["libs"]) link_libs.insert(l.get<string>());
		for (auto& o : sa["objs"]) link_objs.insert(o.get<string>());
		string id = asts[i]["module"];
		if (!isLibraryModule(id)) {
			program_shapes.add(sa);
			continue;
		}
		PlnAllocShapes shapes;
		shapes.add(sa);
		if (shapes.empty()) continue;
		string alloc_o;
		if (int ret = buildAllocators(shapes, palan_work_path + target_files[i] + "." + mode + ".alloc", id, // LCOV_EXCL_BR_LINE
				mode, exec_path, target, tools_time, module_ids, alloc_o))
			return ret;
		obj_files.push_back(alloc_o);
		lib_members.push_back({allocatorsModuleOf(id), alloc_o});
	} // LCOV_EXCL_BR_LINE -- closing brace of a loop body with local strings/json; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

	if (do_lib)
		return buildLibrary(asts, palan_work_path + target_files[0] + "." + mode, obj_files, lib_members, link_objs, binary_name, exec_path, target, library_args);

	// The entry is chosen here, at link time: every module compiles the same
	// whichever role it plays, and only this stub names the first one's
	// initialization and top-level functions.
	fs::create_directories(entry_dir);
	{
		string base = ast_files[0].substr(0, ast_files[0].size() - 9);
		ifstream f(base + "." + mode + ".sa.json");
		json sa = json::parse(f);
		string entry_s = entry_dir + "/entry.s";
		string entry_o = entry_dir + "/entry.o";
		if (int ret = runToolTo({exec_path + "/palan-codegen", "--entry", sa["init"], "--entry", sa["main"]["name"], // LCOV_EXCL_BR_LINE
				"--target", target}, entry_s)) // LCOV_EXCL_BR_LINE
			return ret;
		if (int ret = runToolTo({"as", entry_s}, entry_o)) // LCOV_EXCL_BR_LINE
			return ret;
		obj_files.push_back(entry_o);
	}

	if (!program_shapes.empty()) {
		string alloc_o;
		if (int ret = buildAllocators(program_shapes, entry_dir, "", mode, exec_path, target, tools_time, module_ids, alloc_o))
			return ret;
		obj_files.push_back(alloc_o);
	}

	// ld — link all object files into the final binary. std's rt and -lc are
	// unconditional: code palan-sa and the allocators generate allocates
	// through rt and frees through libc, a runtime dependency independent of
	// any header. link_libs then adds one -l per
	// cinclude `link` clause; names are validated at SA ingest, not here.
	// link_libs is a set, so declaration order from source is already lost --
	// harmless for shared libraries (order doesn't gate symbol resolution).
	// Palan archives have no order either and do depend on each other, hence the group.
	vector<string> ldArgv{"ld"}; // LCOV_EXCL_BR_LINE
	for (auto& obj : obj_files) ldArgv.push_back(obj);
	for (auto& obj : link_objs) ldArgv.push_back(obj);
	if (!archives.empty()) {
		ldArgv.push_back("--start-group");
		ldArgv.insert(ldArgv.end(), archives.begin(), archives.end());
		ldArgv.push_back("--end-group");
	}
	ldArgv.push_back(std_root + "/std/rt.a");
	ldArgv.push_back("-lc");
	for (auto& lib : link_libs) ldArgv.push_back("-l" + lib);
	ldArgv.push_back("-dynamic-linker");
	ldArgv.push_back("/lib64/ld-linux-x86-64.so.2");
	if (output_specified) {
		ldArgv.push_back("-o");
		ldArgv.push_back(binary_name);
		if (int ret = runTool(ldArgv))
			return ret;
	} else if (int ret = runToolTo(ldArgv, binary_name)) {
		return ret;
	}

	if (!output_specified) {
		PlnProcResult r = runProcess(run_argv);
		if (r.status == PlnSpawnStatus::Exited)
			return r.exit_code;
		if (r.status == PlnSpawnStatus::SpawnFailed) {	// LCOV_EXCL_START
			// ld just produced this binary; reaching SpawnFailed here needs an
			// OS-level condition (e.g. a noexec mount, disk full) rather than
			// anything reproducible from Palan source under test.
			cerr << PlnBuildMgrMessage::getMessage(
				E_FailedToExecute, binary_name, strerror(r.err_no)) << endl;
			return 127;
		}	// LCOV_EXCL_STOP
		return -1;
	}

	return 0;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with many local strings/paths; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

string getWorkDirPath(const string& target)
{
	const char* work = getenv("PALAN_WORK");
	if (work && *work)
		return string(work) + "/" + target;

	const char* home = getenv("HOME");
	if (!home || !*home) {
		cerr << PlnBuildMgrMessage::getMessage(E_NoWorkDir) << endl;
		return "";
	}
	return string(home) + "/.palan/work/" + target;
}

