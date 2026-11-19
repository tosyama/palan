/// Generates the allocators' Palan source from the shapes palan-sa records.
///
/// @file PlnAllocatorGen.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include "PlnAllocatorGen.h"
#include "../common/PlnDefs.h"

using namespace std;
using json = nlohmann::json;

void PlnAllocShapes::add(const json& sa)
{
	if (!sa.contains("alloc-shapes")) return;
	for (auto& shape : sa["alloc-shapes"]) {
		string kind = shape.value("shape-kind", "");
		// A bytes or opaque shape is a struct the module only declares.
		bool isStruct = kind == "struct" || kind == "bytes" || kind == "opaque";
		string key = isStruct ? "struct:" + shape["shape-name"].get<string>() : shape["shape-key"].get<string>();
		if (!seen.insert(key).second) continue;
		(isStruct ? structs : arrays).push_back(shape);
	}
}

void writeAllocatorsHeader(ostream& hdr, const PlnAllocShapes& shapes)
{
	// The struct types of the source are named after the program's own, and a
	// system header could bring a C type of any of those names into this
	// module, so it declares only what it calls.
	hdr << "#define NULL ((void *)0)\n"
	    << "void *malloc(unsigned long size);\n"
	    << "void *calloc(unsigned long nmemb, unsigned long size);\n"
	    << "void free(void *ptr);\n"
	    << "void *memcpy(void *dest, const void *src, unsigned long n);\n"
	    << "void *memmove(void *dest, const void *src, unsigned long n);\n"
	    << "void *realloc(void *ptr, unsigned long size);\n"
	    << "void abort(void);\n";
	// A type's '->' is a Palan function, called as C calls it.
	for (auto& shape : shapes.structs) {
		if (shape.contains("copy-func"))
			hdr << "void " << shape["copy-func"].get<string>() << "(const void *src, void *dst);\n";
		if (shape.contains("release-func"))
			hdr << "void " << shape["release-func"].get<string>() << "(void *p);\n";
	}
}

void writeAllocatorsSource(ostream& out, const PlnAllocShapes& shapes, const string& library)
{
	const vector<json>& struct_shapes = shapes.structs;
	const vector<json>& elem_arr_shapes = shapes.arrays;
	if (!library.empty()) out << "library " << library << ";\n";
	out << "cinclude \"" PALAN_ALLOCATORS_MODULE ".h\";\n";

	auto primSize = [](const string& type) -> int64_t {
		static const map<string, int64_t> sizes = {
			{"int8",1}, {"int16",2}, {"int32",4}, {"int64",8}, {"flo32",4}, {"flo64",8}};
		return sizes.at(type);
	};
	map<string, const json*> struct_by_name;
	auto sizeOfInit = [&](const json& init) -> int64_t {
		if (init["kind"] == "structs")
			return init["count"].get<int64_t>() * struct_by_name.at(init["name"])->at("total-size").get<int64_t>();
		return (int64_t)init["values"].size() * primSize(init["type"]);
	};
	for (auto& shape : struct_shapes)
		struct_by_name[shape["shape-name"].get<string>()] = &shape;
	// A struct owning nothing comes only as its layout, a bytes shape.
	auto hasOwned = [&](const string& name) {
		return struct_by_name.at(name)->at("shape-kind") == "struct";
	};
	// Copies one struct's storage into another's: a flat struct is a
	// byte copy, one owning sub-objects goes through its copy function.
	auto structCopy = [&](const string& name, const string& dst, const string& src) {
		if (hasOwned(name))
			return "__pln_copy_" + name + "(" + dst + ", " + src + ");";
		return "memcpy(" + dst + ", " + src + ", "
			+ to_string(struct_by_name.at(name)->at("total-size").get<int64_t>()) + ");";
	};

	// Writes src into the existing dst, both borrows: by the type's '->', or
	// by the function that writes the parts of a struct holding types having one.
	auto structAssigns = [&](const string& name) {
		auto it = struct_by_name.find(name);
		return it != struct_by_name.end() && it->second->value("assigns", false);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto structAssign = [&](const string& name, const string& dst, const string& src) {
		const json& shape = *struct_by_name.at(name);
		if (shape.contains("copy-func"))
			return shape["copy-func"].get<string>() + "(" + src + ", " + dst + ");";
		return "__pln_assign_" + name + "(" + dst + ", " + src + ");";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// Slots are filled with raw C allocations: a borrow slot never takes
	// ownership, and the array's free function releases the elements.
	auto structElemBuild = [&](const string& name, const string& slot, const string& indent) {
		string s = indent + "malloc("
			+ to_string(struct_by_name.at(name)->at("total-size").get<int64_t>())
			+ ") -> " + slot + ";\n";
		if (hasOwned(name))
			s += indent + "__pln_init_" + name + "(" + slot + ");\n";
		return s;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// How the generated code handles one element of an array. An owned
	// element is built when its slot comes into use and dropped when it
	// leaves, through helpers named after the element's type that take
	// the slots as `[]@!T` to build and as `[]T` to free and copy, the
	// views of a struct array's own. Any other element is plain bytes,
	// copied in bulk.
	struct ElemOps {
		string kind;       // the shape entry's element kind
		string inner;      // an inner `[+]T` element's shape key
		string key;        // the element's type symbol
		string type;       // one element's type name in the source
		int64_t size;      // the bytes a slot takes, but a row laid out in place
		int nlens;         // the lengths its helpers take: its levels', then its elements'
		int depth;         // the levels of a row laid out in place
		bool owned;
		bool built;        // built when its slot comes into use: owned, or a struct with initializers
		bool assigns;      // written into by a type's '->', its own or a part's
		bool embedded;     // laid out in the slot itself (`[+]$T`, `[+]$[n]T`)
		string buildElem;  // a slot as the build helper fills it
		string declElem;   // a slot as a local array declares it
		string ownElem;    // a slot as the drop and copy helpers read it
		string slotRun;    // a row block's slots, as `[]` of this; a row laid out in place stands for its elements'
		string fill;       // a row block's slot as its build helper fills it
		string value;      // what push takes
		string popped;     // what pop returns
		string run;        // a copy's source run of elements
		string data;       // the header's data pointer
		string release;    // an opaque element's release function
		shared_ptr<const ElemOps> leaf;   // a row's element
	};
	map<string, const json*> varrElemDescs;
	for (auto& shape : elem_arr_shapes)
		if (shape["shape-kind"] == "varr") varrElemDescs[shape["shape-key"]] = &shape["elem"];
	function<ElemOps(const json&)> opsOf = [&](const json& desc) {
		string kind = desc["kind"];
		ElemOps e;
		e.kind = kind;
		e.inner = desc.value("shape-key", "");
		e.key = desc["key"];
		e.size = desc.value("size", (int64_t)8);
		e.depth = desc.value("depth", 0);
		if (kind == "row" || kind == "embed-row") {
			e.leaf = make_shared<const ElemOps>(opsOf(desc["elem"]));
			const ElemOps& l = *e.leaf;
			e.embedded = kind == "embed-row";
			e.assigns = l.assigns;
			e.value = "[]" + l.slotRun;
			if (!e.embedded) {
				// A row is a block of its own, which the slot points at.
				e.nlens = 1 + l.nlens;
				e.owned = true;
				e.built = true;
				e.buildElem = "@!" + l.fill;
				// A local array of `@!@!T` slots must be spelled `[n]@![]@!T`.
				e.declElem = "@![]" + (l.owned ? l.buildElem : l.slotRun);
				e.ownElem = "[]" + l.slotRun;
				e.slotRun = e.ownElem;
				e.fill = e.buildElem;
				e.popped = e.buildElem;
				e.run = "[]" + e.ownElem;
				e.data = "@!" + e.buildElem;
			} else {
				// A row laid out in place is its elements' slots, depth levels of them.
				// pop moves them into a block of their own for the caller.
				e.nlens = e.depth + l.nlens;
				e.owned = l.owned;
				e.built = l.built;
				e.type = l.type;
				e.buildElem = l.buildElem;
				e.ownElem = l.ownElem;
				e.slotRun = l.slotRun;
				e.fill = l.fill;
				e.popped = "@!" + l.fill;
				e.run = l.run;
				e.data = l.data;
			}
			return e;
		}
		e.type = kind == "prim" ? desc["type"] : desc["name"];
		e.release = desc.value("release-func", "");
		bool isVarr = kind == "varr" || kind == "embed-varr";
		e.nlens = isVarr ? opsOf(*varrElemDescs.at(e.inner)).nlens : 0;
		e.owned = kind == "struct" || kind == "opaque" || isVarr;
		e.embedded = kind == "embed-struct";
		e.built = e.owned || (e.embedded && hasOwned(e.type));
		e.assigns = isVarr ? opsOf(*varrElemDescs.at(e.inner)).assigns // LCOV_EXCL_BR_LINE
		          : kind != "prim" && structAssigns(e.type);
		e.buildElem = "@!" + e.type;
		e.declElem = e.buildElem;
		e.ownElem = e.type;
		e.slotRun = (e.embedded ? "$" : "") + e.type;
		e.fill = e.owned ? e.buildElem : e.type;
		e.value = e.type;
		// A struct is returned by value, as any struct is; another owned element as its block.
		e.popped = e.owned && kind != "struct" ? "@!" + e.type : e.type;
		e.run = e.owned ? "[]" + e.ownElem : "@" + e.type;
		e.data = e.owned ? "@!@!" + e.type : "@!" + e.type;
		return e;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// The helpers of an element take the lengths it needs as m0, m1, ...
	// and pass the rest on to its element's helpers.
	auto lenNames = [](int n, int from = 0) {
		vector<string> names;
		for (int i = from; i < from + n; i++) names.push_back("m" + to_string(i));
		return names;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto lenParams = [&](int n) {
		string s;
		for (auto& name : lenNames(n)) s += ", int64 " + name;
		return s;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto lenArgs = [](const vector<string>& names, size_t from = 0) {
		string s;
		for (size_t i = from; i < names.size(); i++) s += ", " + names[i];
		return s;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto rest = [](const vector<string>& names, size_t from) {
		return vector<string>(names.begin() + from, names.end());
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// The slots a row laid out in place holds, and the bytes any slot takes.
	auto countOf = [](const ElemOps& e, const vector<string>& names) {
		string s = names[0];
		for (int i = 1; i < e.depth; i++) s += " * " + names[i];
		return s;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto sizeOf = [&](const ElemOps& e, const vector<string>& names) {
		if (e.kind != "embed-row") return to_string(e.size);
		return countOf(e, names) + " * " + to_string(e.leaf->size);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// The address of slot idx of a data pointer, for the byte copies of
	// unowned elements. An embedded struct's `arr[i]` is its address already.
	auto slotAddr = [&](const ElemOps& e, const string& arr, const string& idx, const vector<string>& names) {
		bool isRow = e.kind == "embed-row";
		string at = arr + "[" + idx + (isRow ? " * " + countOf(e, names) : "") + "]";
		bool isAddr = isRow ? e.leaf->embedded : e.embedded;
		return (isAddr ? "" : "@!") + at;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// An inner `[+]T` element is its header, starting empty. A row's
	// data starts undefined, as an array variable's does; its owned
	// elements are built at once, so no slot is left unset.
	auto elemBuild = [&](const ElemOps& e, const string& slot, const string& indent, const vector<string>& names) {
		if (e.kind == "embed-varr") return indent + "__pln_init_" + e.inner + "(@!" + slot + ", 0, 0" + lenArgs(names) + ");\n";
		if (e.kind == "embed-row") return indent + "__pln_build_" + e.key + "(" + slot + " as " + e.data + ", 0, 1" + lenArgs(names) + ");\n";
		if (e.kind == "varr") return indent + "calloc(1, 24) -> " + slot + ";\n";
		if (e.kind == "opaque") return indent + "NULL -> " + slot + ";\n";
		if (e.kind == "row") {
			const ElemOps& l = *e.leaf;
			string s = indent + "malloc(" + names[0] + " * " + sizeOf(l, rest(names, 1)) + ") -> " + slot + ";\n";
			if (l.built) s += indent + "__pln_build_" + l.key + "(" + slot + ", 0, " + names[0] + lenArgs(names, 1) + ");\n";
			return s;
		}
		return structElemBuild(e.type, slot, indent);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// A row moved out by `->>` leaves NULL in its slot.
	auto elemDrop = [&](const ElemOps& e, const string& slot, const string& indent, const vector<string>& names) {
		if (e.kind == "embed-varr") return "__pln_fini_" + e.inner + "(@!" + slot + lenArgs(names) + ");";
		if (e.kind == "embed-row") return "__pln_drop_" + e.key + "(" + slot + " as " + e.data + ", 0, 1" + lenArgs(names) + ");";
		if (e.kind == "varr") return "__pln_free_" + e.inner + "(" + slot + lenArgs(names) + ");";
		if (e.kind == "opaque") return e.release + "(" + slot + ");";
		if (e.kind == "row") {
			const ElemOps& l = *e.leaf;
			if (!l.owned) return "free(" + slot + ");";
			return "if " + slot + " != NULL { __pln_drop_" + l.key + "(" + slot
				+ ", 0, " + names[0] + lenArgs(names, 1) + "); }\n" + indent + "free(" + slot + ");";
		}
		return hasOwned(e.type) ? "__pln_free_" + e.type + "(" + slot + ");" : "free(" + slot + ");";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// dst is the slot as its build fills it, a writable reference.
	auto elemSet = [&](const ElemOps& e, const string& dst, const string& src, const vector<string>& names) {
		// SA copies nothing holding an opaque value, whose layout this module does not know.
		if (e.kind == "opaque") return string("abort();");
		if (e.kind == "embed-varr")
			return "__pln_copy_" + e.inner + "(@!" + dst + ", " + src + ".data, " + src + ".len" + lenArgs(names) + ");";
		if (e.kind == "embed-row")
			return "__pln_copyrun_" + e.key + "(" + dst + " as " + e.data + ", " + src + " as " + e.data + ", 1" + lenArgs(names) + ");";
		if (e.kind == "varr")
			return "__pln_copy_" + e.inner + "(" + dst + ", " + src + ".data, " + src + ".len" + lenArgs(names) + ");";
		if (e.kind == "row") {
			const ElemOps& l = *e.leaf;
			if (l.owned) return "__pln_copyrun_" + l.key + "(" + dst + ", " + src + ", " + names[0] + lenArgs(names, 1) + ");";
			return "memcpy(" + dst + ", " + src + ", " + names[0] + " * " + sizeOf(l, rest(names, 1)) + ");";
		}
		return structCopy(e.type, dst, src);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// An owned struct slot (an inner `[+]T`'s header is one) is passed to
	// the helpers as a borrow, since a struct parameter would own and free it.
	auto borrowed = [](const ElemOps& e, const string& v, bool isMutable = false) {
		return e.kind == "struct" || e.kind == "varr" ? (isMutable ? "@!" : "@") + v : v;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// As elemSet, into a slot already holding a value: the parts a type's '->'
	// writes are written by it.
	auto elemAssign = [&](const ElemOps& e, const string& dst, const string& src, const vector<string>& names) {
		if (!e.assigns) return elemSet(e, dst, src, names);
		if (e.kind == "embed-varr")
			return "__pln_assign_" + e.inner + "(@!" + dst + ", " + src + ".data, " + src + ".len" + lenArgs(names) + ");";
		if (e.kind == "varr")
			return "__pln_assign_" + e.inner + "(" + dst + ", " + src + ".data, " + src + ".len" + lenArgs(names) + ");";
		if (e.kind == "embed-row")
			return "__pln_assignrun_" + e.key + "(" + dst + " as " + e.data + ", " + src + " as " + e.data + ", 1" + lenArgs(names) + ");";
		if (e.kind == "row" && !e.leaf->owned)
			return "__pln_assignrun_" + e.leaf->key + "(" + dst + " as " + e.leaf->data + ", " + src + " as " + e.leaf->run
				+ ", " + names[0] + lenArgs(names, 1) + ");";
		if (e.kind == "row")
			return "__pln_assignrun_" + e.leaf->key + "(" + dst + ", " + src + ", " + names[0] + lenArgs(names, 1) + ");";
		return structAssign(e.type, dst, src);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// A slot pointing at a block of its own is NULL once `->>` has moved it out.
	auto slotMayBeNull = [](const ElemOps& e) {
		return e.kind == "struct" || e.kind == "varr" || e.kind == "row";
	};
	// The body of a run's copy loop: a NULL source element leaves the slot
	// NULL, and a NULL slot is built before it is copied into whole.
	auto keepingNull = [&](const ElemOps& e, const string& copy, const string& ma) {
		string set = "__pln_set_" + e.key + "(dst, i, " + borrowed(e, "src[i]") + ma + ");";
		if (!slotMayBeNull(e)) return "        " + copy + "\n";
		string slots = "dst as " + e.data;
		string build = "__pln_build_" + e.key + "(" + slots + ", i, i + 1" + ma + ");";
		string fill = copy == set ? "            if dst[i] == NULL { " + build + " }\n            " + set + "\n"
		            : "            if dst[i] == NULL { " + build + " " + set + " }\n            else { " + copy + " }\n";
		return "        if src[i] == NULL { __pln_clear_" + e.key + "(" + slots + ", i" + ma + "); }\n"
		       "        else {\n" + fill + "        }\n";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	set<string> elemHelpersDone;
	function<void(const ElemOps&)> emitElemHelpers = [&](const ElemOps& e) {
		if (!e.built || !elemHelpersDone.insert(e.key).second) return;
		if (e.leaf) emitElemHelpers(*e.leaf);
		// Structs laid out in place own nothing: they are only built.
		if (!e.owned) {
			string l = e.kind == "embed-row" ? e.leaf->key : "";
			vector<string> names = lenNames(e.nlens);
			string n = e.kind == "embed-row" ? countOf(e, names) : "";
			// The slots come as the data pointer an array of them keeps.
			out << "\nfunc __pln_build_" << e.key << "(@!" << e.type << " slots, int64 from, int64 to"
			    << lenParams(e.nlens) << ") {\n";
			if (e.kind == "embed-row") out << "    __pln_build_" << l << "(slots, from * " << n << ", to * " << n << ");\n";
			else out << "    __pln_initrun_" << e.type << "(slots, from, to);\n";
			out << "}\n";
			return;
		}
		vector<string> names = lenNames(e.nlens);
		string mp = lenParams(e.nlens), ma = lenArgs(names);
		string head = "\nfunc __pln_build_" + e.key + "([]" + e.buildElem + " slots, int64 from, int64 to" + mp + ") {\n";
		string dropHead = "func __pln_drop_" + e.key + "([]" + e.ownElem + " slots, int64 from, int64 to" + mp + ") {\n";
		string setHead = "func __pln_set_" + e.key + "([]" + e.ownElem + " slots, int64 i, " + borrowed(e, e.value) + " x" + mp + ") {\n";
		string runHead = "func __pln_copyrun_" + e.key + "([]" + e.ownElem + " dst, []" + e.ownElem + " src, int64 n" + mp + ") {\n";
		if (e.kind == "embed-row") {
			// Rows laid out in place are runs of their elements' slots.
			string l = e.leaf->key, n = countOf(e, names), la = lenArgs(names, e.depth);
			out << head
			    << "    __pln_build_" << l << "(slots, from * " << n << ", to * " << n << la << ");\n"
			    << "}\n"
			    << dropHead
			    << "    __pln_drop_" << l << "(slots, from * " << n << ", to * " << n << la << ");\n"
			    << "}\n"
			    << setHead
			    << "    int64 j = 0;\n"
			    << "    while j < " << n << " {\n"
			    << "        __pln_set_" << l << "(slots, i * " << n << " + j, " << borrowed(*e.leaf, "x[j]") << la << ");\n"
			    << "        j + 1 -> j;\n"
			    << "    }\n"
			    << "}\n"
			    << runHead
			    << "    __pln_copyrun_" << l << "(dst, src, n * " << n << la << ");\n"
			    << "}\n";
			return;
		}
		out << head
		    << "    int64 i = from;\n"
		    << "    while i < to {\n"
		    << elemBuild(e, "slots[i]", "        ", names)
		    << "        i + 1 -> i;\n"
		    << "    }\n"
		    << "}\n"
		    << dropHead
		    << "    int64 i = from;\n"
		    << "    while i < to {\n"
		    << "        " << elemDrop(e, borrowed(e, "slots[i]", true), "        ", names) << "\n"
		    << "        i + 1 -> i;\n"
		    << "    }\n"
		    << "}\n"
		    << setHead
		    << "    " << elemSet(e, "@!slots[i]", "x", names) << "\n"
		    << "}\n"
		    << runHead
		    << "    int64 i = 0;\n"
		    << "    while i < n {\n"
		    << keepingNull(e, "__pln_set_" + e.key + "(dst, i, " + borrowed(e, "src[i]") + ma + ");", ma)
		    << "        i + 1 -> i;\n"
		    << "    }\n"
		    << "}\n";
		if (slotMayBeNull(e))
			out << "func __pln_clear_" << e.key << "([]" << e.buildElem << " slots, int64 i" << mp << ") {\n"
			    << "    " << elemDrop(e, "slots[i]", "    ", names) << "\n"
			    << "    NULL -> slots[i];\n"
			    << "}\n";
	};

	// __pln_assignrun_E writes a run of elements into a run of existing ones.
	// Elements laid out in place are addressed in their run, as `@!dst[i]`.
	set<string> assignHelpersDone;
	function<void(const ElemOps&)> emitAssignHelpers = [&](const ElemOps& e) {
		if (!e.assigns || !assignHelpersDone.insert(e.key).second) return;
		if (e.leaf) emitAssignHelpers(*e.leaf);
		if (slotMayBeNull(e)) emitElemHelpers(e);
		vector<string> names = lenNames(e.nlens);
		string mp = lenParams(e.nlens), ma = lenArgs(names);
		string dstRun = e.owned ? "[]" + e.ownElem : e.data;
		string srcRun = e.owned ? "[]" + e.ownElem : e.run;
		out << "\nfunc __pln_assignrun_" << e.key << "(" << dstRun << " dst, " << srcRun << " src, int64 n" << mp << ") {\n";
		if (e.kind == "embed-row") {
			string n = countOf(e, names);
			out << "    __pln_assignrun_" << e.leaf->key << "(dst, src, n * " << n << lenArgs(names, e.depth) << ");\n"
			    << "}\n";
			return;
		}
		out << "    int64 i = 0;\n"
		    << "    while i < n {\n";
		if (e.kind == "embed-struct")
			out << "        " << structAssign(e.type, "@!dst[i]", "@src[i]") << "\n";
		else
			out << keepingNull(e, "__pln_aset_" + e.key + "(dst, i, " + borrowed(e, "src[i]") + ma + ");", ma);
		out << "        i + 1 -> i;\n"
		    << "    }\n"
		    << "}\n";
		if (e.kind != "embed-struct")
			out << "func __pln_aset_" << e.key << "([]" << e.ownElem << " slots, int64 i, " << borrowed(e, e.value) << " x" << mp << ") {\n"
			    << "    " << elemAssign(e, "@!slots[i]", "x", names) << "\n"
			    << "}\n";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// Struct type declarations (leaf-first order from SA).
	// Only the owned pointer fields are declared, at their original
	// offsets; every other byte range becomes opaque padding. The
	// allocators touch nothing else, and the full field list cannot
	// be re-expressed as Palan source in general (C unions, synthesized
	// anonymous tags, embedded types not declared in this module).
	// Their names are the identities the shapes carry, which SA keeps for
	// this module's types.
	// Byte ranges of each struct outside its owned pointer fields,
	// declared as __pln_padN fields and copied as they are.
	map<string, vector<pair<string, int64_t>>> struct_pads;
	for (auto& shape : struct_shapes) {
		string name = shape["shape-name"];
		// An owned field is declared as the slot its build fills, so
		// this module neither copies nor frees what it is given.
		// A row laid out in place is a run of its elements' slots, which its
		// helpers take as a data pointer, as a `[+]T`'s, since an array of
		// these slots would not pass where owned ones are expected.
		// A `[+]T` header laid out in the struct is declared as the header
		// type, which owns nothing here. The slots are named by position, since
		// a nested struct's owned fields come with the path to them.
		vector<tuple<int64_t, string, int64_t>> owned_decls;
		int slot_no = 0;
		for (auto& of : shape.value("owned-fields", json::array())) {
			ElemOps e = opsOf(of["elem"]);
			string slot = "__pln_o" + to_string(slot_no++);
			if (e.kind == "embed-varr") {
				owned_decls.push_back({of["offset"].get<int64_t>(), "$" + e.type + " " + slot, e.size});
				continue;
			}
			int64_t count = 1;
			for (int i = 0; i < e.depth; i++) count *= of["lens"][i].get<int64_t>();
			string run = e.kind == "embed-row" ? "$[" + to_string(count) + "]" : "";
			owned_decls.push_back({of["offset"].get<int64_t>(), run + e.buildElem + " " + slot, count * 8});
		}
		// What an initializer stores in place is declared as its type, so the
		// stores and copies go through it.
		int init_no = 0;
		for (auto& init : shape.value("inits", json::array())) {
			string kind = init["kind"], slot = "__pln_i" + to_string(init_no++);
			int64_t offset = init["offset"];
			if (kind == "prim")
				owned_decls.push_back({offset, init["type"].get<string>() + " " + slot, primSize(init["type"])});
			else if (kind == "run")
				owned_decls.push_back({offset, "$[" + to_string(init["values"].size()) + "]" + init["type"].get<string>() + " " + slot,
					(int64_t)init["values"].size() * primSize(init["type"])});
			else if (kind == "structs")
				owned_decls.push_back({offset, "$[" + to_string(init["count"].get<int64_t>()) + "]$" + init["name"].get<string>() + " " + slot,
					init["count"].get<int64_t>() * struct_by_name.at(init["name"])->at("total-size").get<int64_t>()});
		}
		// A run of structs a type's '->' writes is declared as their type, so
		// that their own functions take each.
		int run_no = 0;
		for (auto& run : shape.value("runs", json::array())) {
			string sub = run["name"];
			int64_t count = run["count"];
			owned_decls.push_back({run["offset"].get<int64_t>(),
				"$[" + to_string(count) + "]$" + sub + " __pln_r" + to_string(run_no++),
				count * struct_by_name.at(sub)->at("total-size").get<int64_t>()});
		}
		sort(owned_decls.begin(), owned_decls.end());

		out << "\nexport type " << name << " {";
		int64_t pos = 0;
		int pad_no = 0;
		auto pad_to = [&](int64_t end) {
			if (end <= pos) return;
			string pad = "__pln_pad" + to_string(pad_no++);
			out << " $[" << (end - pos) << "]uint8 " << pad << ";";
			struct_pads[name].push_back({pad, end - pos});
		};
		for (auto& [offset, decl, size] : owned_decls) {
			pad_to(offset);
			out << " " << decl << ";";
			pos = offset + size;
		}
		pad_to(shape["total-size"].get<int64_t>());
		out << " };\n";
	}

	// What `stmt` does to each struct of every run, `{}` standing for the run.
	auto forRuns = [&](const json& shape, const function<string(const string&, const string&)>& stmt) {
		string s;
		int run_no = 0;
		for (auto& run : shape.value("runs", json::array())) {
			string slot = "__pln_r" + to_string(run_no++) + "[i]";
			s += "    {\n"
			     "        int64 i = 0;\n"
			     "        while i < " + to_string(run["count"].get<int64_t>()) + " {\n"
			     "            " + stmt(run["name"].get<string>(), slot) + "\n"
			     "            i + 1 -> i;\n"
			     "        }\n"
			     "    }\n";
		}
		return s;
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// Struct allocator/free functions, for structs owning fields
	for (auto& shape : struct_shapes) {
		if (shape["shape-kind"] == "bytes" || shape["shape-kind"] == "opaque") continue;
		string name = shape["shape-name"];
		struct OwnedField { string slot; ElemOps e; vector<string> lens; };
		vector<OwnedField> owned;
		for (auto& of : shape["owned-fields"]) {
			OwnedField f{"__pln_o" + to_string(owned.size()), opsOf(of["elem"]), {}}; // LCOV_EXCL_BR_LINE
			for (auto& len : of["lens"]) f.lens.push_back(to_string(len.get<int64_t>()));
			if (f.e.kind == "embed-row") emitElemHelpers(f.e);
			else if (f.e.leaf) emitElemHelpers(*f.e.leaf);
			if (f.e.kind == "embed-row") emitAssignHelpers(f.e);
			else if (f.e.leaf) emitAssignHelpers(*f.e.leaf);
			owned.push_back(move(f));
		}

		// The block is returned as `@!T`: a `T` return would be allocated
		// by this very function.
		out << "\nexport func __pln_alloc_" << name
		    << "() -> @!" << name << " p {\n"
		    << "    malloc(" << shape["total-size"].get<int64_t>() << ") -> p;\n"
		    << "    __pln_init_" << name << "(p);\n"
		    << "}\n";

		// The initializers are stored once the owned fields are built, since
		// a `[+]T` one is resized first.
		map<int64_t, string> slotAt;
		for (size_t i = 0; i < owned.size(); i++)
			slotAt[shape["owned-fields"][i]["offset"].get<int64_t>()] = owned[i].slot;
		const json noInits = json::array();
		const json& inits = shape.contains("inits") ? shape["inits"] : noInits;
		// A struct in a stack frame releases only what it owns (fini);
		// a heap one also frees its own block.
		out << "export func __pln_init_" << name << "(@!" << name << " p) {\n";
		for (auto& f : owned) out << elemBuild(f.e, "p." + f.slot, "    ", f.lens);
		out << forRuns(shape, [](const string& sub, const string& slot) { return "__pln_init_" + sub + "(@!p." + slot + ");"; });
		for (size_t i = 0; i < inits.size(); i++) {
			const json& init = inits[i];
			string kind = init["kind"], slot = "p.__pln_i" + to_string(i);
			if (kind == "structs") {
				out << "    __pln_initrun_" << init["name"].get<string>() << "(@!" << slot << "[0], 0, "
				    << init["count"].get<int64_t>() << ");\n";
				continue;
			}
			const json& values = init["values"];
			if (kind == "prim") {
				out << "    " << values[0].get<string>() << " -> " << slot << ";\n";
				continue;
			}
			string data = slot;
			if (kind != "run") {
				string owner = "p." + slotAt.at(init["offset"].get<int64_t>());
				data = owner;
				if (kind == "varr" || kind == "embed-varr") {
					out << "    __pln_resize_" << init["shape-key"].get<string>() << "("
					    << (kind == "embed-varr" ? "@!" : "") << owner << ", " << values.size() << ");\n";
					data = owner + ".data";
				}
			}
			for (size_t j = 0; j < values.size(); j++)
				out << "    " << values[j].get<string>() << " -> " << data << "[" << j << "];\n";
		}
		out << "}\n"
		    << "export func __pln_fini_" << name << "(@!" << name << " p) {\n";
		for (auto& f : owned) out << "    " << elemDrop(f.e, "p." + f.slot, "    ", f.lens) << "\n";
		out << forRuns(shape, [](const string& sub, const string& slot) { return "__pln_fini_" + sub + "(@!p." + slot + ");"; });
		out << "}\n"
		    << "export func __pln_free_" << name
		    << "(@!" << name << " p) {\n"
		    << "    if (p == NULL) { return; }\n"
		    << "    __pln_fini_" << name << "(p);\n"
		    << "    free(p);\n"
		    << "    return;\n"
		    << "}\n";

		out << "export func __pln_copy_" << name
		    << "(@!" << name << " dst, @" << name << " src) {\n";
		for (auto& [pad, size] : struct_pads[name])
			out << "    memcpy(dst." << pad << ", src." << pad << ", " << size << ");\n";
		for (auto& f : owned)
			out << "    " << elemSet(f.e, "dst." + f.slot, "src." + f.slot, f.lens) << "\n";
		for (size_t i = 0; i < inits.size(); i++) {
			string slot = "__pln_i" + to_string(i);
			if (inits[i]["kind"] == "prim") out << "    src." << slot << " -> dst." << slot << ";\n";
			else if (inits[i]["kind"] == "run" || inits[i]["kind"] == "structs")
				out << "    memcpy(dst." << slot << ", src." << slot << ", " << sizeOfInit(inits[i]) << ");\n";
		}
		out << forRuns(shape, [](const string& sub, const string& slot) {
			return "__pln_copy_" + sub + "(@!dst." + slot + ", @src." + slot + ");";
		});
		out << "}\n";
		// Into an existing value: as the copy, but for what a type's '->' writes.
		if (shape.value("assigns", false) && !shape.contains("copy-func")) {
			out << "export func __pln_assign_" << name << "(@!" << name << " dst, @" << name << " src) {\n";
			for (auto& [pad, size] : struct_pads[name])
				out << "    memcpy(dst." << pad << ", src." << pad << ", " << size << ");\n";
			for (auto& f : owned)
				out << "    " << elemAssign(f.e, "dst." + f.slot, "src." + f.slot, f.lens) << "\n";
			for (size_t i = 0; i < inits.size(); i++) {
				string slot = "__pln_i" + to_string(i);
				if (inits[i]["kind"] == "prim") out << "    src." << slot << " -> dst." << slot << ";\n";
				else if (inits[i]["kind"] == "run" || inits[i]["kind"] == "structs")
					out << "    memcpy(dst." << slot << ", src." << slot << ", " << sizeOfInit(inits[i]) << ");\n";
			}
			out << forRuns(shape, [&](const string& sub, const string& slot) {
				return structAssign(sub, "@!dst." + slot, "@src." + slot);
			});
			out << "}\n";
		}
		// A run of these laid out in place, when they own nothing: built where
		// a field initializer or a type's '->' is its own.
		if (owned.empty())
			out << "export func __pln_initrun_" << name << "(@!" << name << " run, int64 from, int64 to) {\n"
			    << "    int64 i = from;\n"
			    << "    while i < to {\n"
			    << "        __pln_init_" << name << "(@!run[i]);\n"
			    << "        i + 1 -> i;\n"
			    << "    }\n"
			    << "}\n";
	}

	for (auto& shape : elem_arr_shapes) {
		ElemOps e = opsOf(shape["elem"]);
		string key = shape["shape-key"];
		emitElemHelpers(e);
		vector<string> names = lenNames(e.nlens);
		string mp = lenParams(e.nlens), ma = lenArgs(names);
		if (shape["shape-kind"] == "arr" && !e.owned) {
			// Elements laid out in place, copied as bytes: only written into.
			emitAssignHelpers(e);
			out << "\nexport func __pln_assign_" << key << "(" << e.data << " dst, " << e.run << " src, int64 n" << mp << ") {\n"
			    << "    __pln_assignrun_" << e.key << "(dst, src, n" << ma << ");\n"
			    << "}\n";
			continue;
		}
		if (shape["shape-kind"] == "arr") {
			// A fixed array of owned elements, all of them live.
			string buildRun = "[]" + e.buildElem, ownRun = "[]" + e.ownElem;
			out << "\nexport func __pln_init_" << key << "(" << buildRun << " outer, int64 n" << mp << ") {\n"
			    << "    __pln_build_" << e.key << "(outer, 0, n" << ma << ");\n"
			    << "}\n"
			    << "export func __pln_alloc_" << key << "(int64 n" << mp << ") -> []" << e.declElem << " {\n"
			    << "    [n]" << e.declElem << " outer;\n"
			    << "    __pln_init_" << key << "(outer, n" << ma << ");\n"
			    << "    return outer;\n"
			    << "}\n"
			    << "export func __pln_fini_" << key << "(" << ownRun << " pts, int64 n" << mp << ") {\n"
			    << "    __pln_drop_" << e.key << "(pts, 0, n" << ma << ");\n"
			    << "}\n"
			    << "export func __pln_free_" << key << "(" << ownRun << " pts, int64 n" << mp << ") {\n"
			    << "    if (pts == NULL) { return; }\n"
			    << "    __pln_fini_" << key << "(pts, n" << ma << ");\n"
			    << "    free(pts);\n"
			    << "    return;\n"
			    << "}\n"
			    << "export func __pln_copy_" << key << "(" << ownRun << " dst, " << ownRun << " src, int64 n" << mp << ") {\n"
			    << "    __pln_copyrun_" << e.key << "(dst, src, n" << ma << ");\n"
			    << "}\n";
			if (e.assigns) {
				emitAssignHelpers(e);
				out << "export func __pln_assign_" << key << "(" << ownRun << " dst, " << ownRun << " src, int64 n" << mp << ") {\n"
				    << "    __pln_assignrun_" << e.key << "(dst, src, n" << ma << ");\n"
				    << "}\n";
			}
			continue;
		}
		// A variable-length array's header {len, cap, data}. Only the
		// elements below len are live: an owned element is built when the
		// length grows over its slot and dropped when it shrinks below it.
		string hdr = "__pln_" + key;
		string size = sizeOf(e, names);
		out << "\nexport type " << hdr << " { int64 len; int64 cap; " << e.data << " data; };\n";
		out << "export func __pln_init_" << key
		    << "(@!" << hdr << " h, int64 len, int64 cap" << mp << ") {\n"
		    << "    len -> h.len;\n"
		    << "    cap -> h.cap;\n"
		    << "    if cap > 0 { malloc(cap * " << size << ") -> h.data; }\n"
		    << "    else { NULL -> h.data; }\n";
		if (e.built) out << "    __pln_build_" << e.key << "(h.data, 0, len" << ma << ");\n";
		out << "}\n"
		// The header is returned as its block, as a struct allocator's is.
		    << "export func __pln_alloc_" << key
		    << "(int64 len, int64 cap" << mp << ") -> @!" << hdr << " h {\n"
		    << "    malloc(24) -> h;\n"
		    << "    __pln_init_" << key << "(h, len, cap" << ma << ");\n"
		    << "}\n"
		    << "export func __pln_fini_" << key << "(@!" << hdr << " h" << mp << ") {\n";
		if (e.owned) out << "    __pln_drop_" << e.key << "(h.data, 0, h.len" << ma << ");\n";
		out << "    free(h.data);\n"
		    << "}\n"
		    << "export func __pln_free_" << key << "(@!" << hdr << " h" << mp << ") {\n"
		    << "    if (h == NULL) { return; }\n"
		    << "    __pln_fini_" << key << "(h" << ma << ");\n"
		    << "    free(h);\n"
		    << "    return;\n"
		    << "}\n"
		// Growing doubles the capacity so that adding one element at a
		// time, by push or by resize, reallocates only now and then.
		    << "func __pln_reserve_" << key << "(@!" << hdr << " h, int64 n" << mp << ") {\n"
		    << "    int64 c = h.cap * 2;\n"
		    << "    if c < n { n -> c; }\n"
		    << "    if c < 4 { 4 -> c; }\n"
		    << "    realloc(h.data, c * " << size << ") -> h.data;\n"
		    << "    c -> h.cap;\n"
		    << "}\n";
		// A struct or a row (an inner `[+]T`'s header too) comes as the
		// caller's to own, as a parameter of its type takes its argument: a
		// block of its own becomes the slot, and one laid out in place has
		// what it holds moved into the slots and is freed.
		bool movesSlots = e.kind == "embed-row" && e.owned;
		string pushed = movesSlots ? e.popped : (e.kind != "prim" ? "@!" : "") + e.value;
		out << "export func __pln_push_" << key << "(@!" << hdr << " h, " << pushed << " x" << mp << ") {\n"
		    << "    if h.len == h.cap { __pln_reserve_" << key << "(h, h.len + 1" << ma << "); }\n";
		// Owned elements' slots are pointers, which '@!' cannot address.
		if (movesSlots) {
			string n = countOf(e, names);
			out << "    int64 j = 0;\n"
			    << "    while j < " << n << " {\n"
			    << "        x[j] -> h.data[h.len * " << n << " + j];\n"
			    << "        j + 1 -> j;\n"
			    << "    }\n"
			    << "    free(x);\n";
		} else if (e.embedded)
			out << "    memcpy(" << slotAddr(e, "h.data", "h.len", names) << ", x, " << size << ");\n"
			    << "    free(x);\n";
		else
			out << "    x -> h.data[h.len];\n";
		out << "    h.len + 1 -> h.len;\n"
		    << "}\n"
		    << "export func __pln_resize_" << key << "(@!" << hdr << " h, int64 n" << mp << ") {\n"
		    << "    if n < 0 { abort(); }\n"
		    << "    if n > h.cap { __pln_reserve_" << key << "(h, n" << ma << "); }\n";
		if (e.owned)
			out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n" << ma << "); }\n"
			    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len" << ma << "); }\n";
		else if (e.built)
			out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n" << ma << "); }\n";
		out << "    n -> h.len;\n"
		    << "}\n"
		// The source is any run of elements, which may be h's own
		// (`v -> v`, or a borrow of v copied into v); then n is h's
		// length, so no element is built, freed or moved.
		    << "export func __pln_copy_" << key << "(@!" << hdr << " h, " << e.run << " src, int64 n" << mp << ") {\n"
		    << "    if n > h.cap { __pln_reserve_" << key << "(h, n" << ma << "); }\n";
		if (e.owned)
			out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n" << ma << "); }\n"
			    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len" << ma << "); }\n"
			    << "    __pln_copyrun_" << e.key << "(h.data, src, n" << ma << ");\n";
		else
			out << "    memmove(h.data, src, n * " << size << ");\n";
		out << "    n -> h.len;\n"
		    << "}\n";
		// The elements h has are written into, and those it gains are copied
		// whole, as new values; src may be h's own elements, as for the copy.
		if (e.assigns) {
			emitAssignHelpers(e);
			out << "export func __pln_assign_" << key << "(@!" << hdr << " h, " << e.run << " src, int64 n" << mp << ") {\n"
			    << "    if n > h.cap { __pln_reserve_" << key << "(h, n" << ma << "); }\n"
			    << "    int64 m = h.len;\n"
			    << "    if n > m {\n";
			if (e.built) out << "        __pln_build_" << e.key << "(h.data, m, n" << ma << ");\n";
			if (e.owned) {
				// A row laid out in place is no slot of its own, but its elements' slots.
				bool isRow = e.kind == "embed-row";
				const ElemOps& s = isRow ? *e.leaf : e;
				string per = isRow ? " * " + countOf(e, names) : "";
				out << "        int64 i = m" << per << ";\n"
				    << "        while i < n" << per << " {\n"
				    << "            __pln_set_" << s.key << "(h.data, i, " << borrowed(s, "src[i]")
				    << (isRow ? lenArgs(names, e.depth) : ma) << ");\n"
				    << "            i + 1 -> i;\n"
				    << "        }\n";
			} else
				out << "        memcpy(" << slotAddr(e, "h.data", "m", names) << ", " << slotAddr(e, "src", "m", names)
				    << ", (n - m) * " << size << ");\n";
			out << "    } else {\n";
			if (e.owned) out << "        __pln_drop_" << e.key << "(h.data, n, m" << ma << ");\n";
			out << "        n -> m;\n"
			    << "    }\n"
			    << "    __pln_assignrun_" << e.key << "(h.data, src, m" << ma << ");\n"
			    << "    n -> h.len;\n"
			    << "}\n";
		}
		// An owned struct moves out of its block, which return then frees.
		if (e.kind == "struct") {
			out << "export func __pln_pop_" << key << "(@!" << hdr << " h" << mp << ") -> " << e.type << " {\n"
			    << "    if h.len == 0 { abort(); }\n"
			    << "    h.len - 1 -> h.len;\n"
			    << "    " << e.type << " x <<= h.data[h.len] as " << e.type << ";\n"
			    << "    return x;\n"
			    << "}\n";
			continue;
		}
		// An owned element leaves its slot for the caller.
		out << "export func __pln_pop_" << key << "(@!" << hdr << " h" << mp << ") -> " << e.popped << " x {\n"
		    << "    if h.len == 0 { abort(); }\n"
		    << "    h.len - 1 -> h.len;\n";
		if (e.kind == "embed-row")
			out << "    malloc(" << size << ") -> x;\n";
		// Owned elements' slots are pointers, which '@!' cannot address.
		if (e.kind == "embed-row" && e.owned) {
			string n = countOf(e, names);
			out << "    int64 j = 0;\n"
			    << "    while j < " << n << " {\n"
			    << "        h.data[h.len * " << n << " + j] -> x[j];\n"
			    << "        j + 1 -> j;\n"
			    << "    }\n";
		} else if (e.embedded) {
			out << "    memcpy(x, " << slotAddr(e, "h.data", "h.len", names) << ", " << size << ");\n";
		} else {
			out << "    h.data[h.len] -> x;\n";
		}
		out << "}\n";
	}
}
