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
	    << "void *__pln_malloc(unsigned long size);\n"
	    << "void *__pln_calloc(unsigned long nmemb, unsigned long size);\n"
	    << "void *__pln_realloc(void *ptr, unsigned long size);\n"
	    << "void __pln_pop_empty(void);\n"
	    << "void __pln_negative_size(void);\n"
	    << "void free(void *ptr);\n"
	    << "void *memcpy(void *dest, const void *src, unsigned long n);\n"
	    << "void *memmove(void *dest, const void *src, unsigned long n);\n"
	    << "void *memset(void *s, int c, unsigned long n);\n"
	    << "void abort(void);\n";
	bool hasDict = any_of(shapes.arrays.begin(), shapes.arrays.end(),
	                      [](const json& s) { return s["shape-kind"] == "dict"; });
	if (hasDict)
		hdr << "void *__pln_dict_new(void);\n"
		    << "void __pln_dict_free(void *h);\n"
		    << "void __pln_dict_copy(void *dst, const void *src);\n"
		    << "const unsigned char *__pln_dict_key(const void *h, long i);\n"
		    << "long __pln_dict_find(const void *h, const unsigned char *k);\n"
		    << "long __pln_dict_at(const void *h, const unsigned char *k);\n"
		    << "long __pln_dict_add(void *h, const unsigned char *k);\n"
		    << "long __pln_dict_remove(void *h, const unsigned char *k);\n"
		    << "void __pln_dict_settle(void *h, long elemSize);\n"
		    << "void __pln_dict_clear(void *h);\n";
	// A type's '->' and the function storing its field initializers are
	// Palan functions, called as C calls them.
	for (auto& shape : shapes.structs) {
		if (shape.contains("copy-func"))
			hdr << "void " << shape["copy-func"].get<string>() << "(const void *src, void *dst);\n";
		if (shape.contains("init-func"))
			hdr << "void " << shape["init-func"].get<string>() << "(void *p);\n";
		if (shape.contains("release-func"))
			hdr << "void " << shape["release-func"].get<string>() << "(void *p);\n";
	}
}

static string variantTypeName(const json& unionShape, size_t k)
{
	return "__pln_v" + to_string(k) + "_" + unionShape["shape-name"].get<string>();
}

static bool variantOwns(const json& unionShape, size_t k)
{
	const json& v = unionShape["variants"][k];
	return !v["owned-fields"].empty() || !v["runs"].empty();
}

void writeAllocatorsSource(ostream& out, const PlnAllocShapes& shapes, const string& library, bool checks)
{
	// A union's variants that own something are each laid out as a struct
	// of its own, its contents at their place and the rest bytes, which the
	// union's functions view it as for the variant its tag names.
	vector<json> struct_shapes;
	for (auto& shape : shapes.structs) {
		if (shape.contains("variants")) {
			const json& variants = shape["variants"];
			for (size_t k = 0; k < variants.size(); k++) {
				if (variants[k]["owned-fields"].empty() && variants[k]["runs"].empty()) continue;
				json v = {{"shape-kind", "struct"}, {"shape-name", variantTypeName(shape, k)},
				          {"total-size", shape["total-size"]}, {"owned-fields", variants[k]["owned-fields"]}}; // LCOV_EXCL_BR_LINE
				if (!variants[k]["runs"].empty()) v["runs"] = variants[k]["runs"];
				v["assigns"] = true;
				struct_shapes.push_back(move(v));
			}
		}
		struct_shapes.push_back(shape);
	}
	const vector<json>& elem_arr_shapes = shapes.arrays;
	if (!library.empty()) out << "library " << library << ";\n";
	out << "cinclude \"" PALAN_ALLOCATORS_MODULE ".h\";\n";

	map<string, const json*> struct_by_name;
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
	// One with neither, laid out in a union's variant, is released and copied.
	auto structAssigns = [&](const string& name) {
		auto it = struct_by_name.find(name);
		return it != struct_by_name.end() && it->second->value("assigns", false);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	auto structAssign = [&](const string& name, const string& dst, const string& src) {
		const json& shape = *struct_by_name.at(name);
		if (shape.contains("copy-func"))
			return shape["copy-func"].get<string>() + "(" + src + ", " + dst + ");";
		if (!shape.value("assigns", false))
			return "__pln_fini_" + name + "(" + dst + "); __pln_copy_" + name + "(" + dst + ", " + src + ");";
		return "__pln_assign_" + name + "(" + dst + ", " + src + ");";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// Slots are filled with raw C allocations: a borrow slot never takes
	// ownership, and the array's free function releases the elements.
	auto structElemBuild = [&](const string& name, const string& slot, const string& indent) {
		string s = indent + "__pln_malloc("
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
	map<string, string> dictValues;
	for (auto& shape : elem_arr_shapes) {
		if (shape["shape-kind"] == "varr") varrElemDescs[shape["shape-key"]] = &shape["elem"];
		if (shape["shape-kind"] == "dict") dictValues[shape["shape-key"]] = shape["values"];
	}
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
		// A dictionary's functions take the lengths its values' do.
		const json* values = isVarr ? varrElemDescs.at(e.inner)
		                   : kind == "dict" ? varrElemDescs.at(dictValues.at(e.inner)) : nullptr;
		e.nlens = values ? opsOf(*values).nlens : 0;
		e.owned = kind == "struct" || kind == "opaque" || kind == "dict" || isVarr;
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
		if (e.kind == "varr") return indent + "__pln_calloc(1, 24) -> " + slot + ";\n";
		if (e.kind == "dict") return indent + "__pln_dict_new() -> " + slot + ";\n";
		if (e.kind == "opaque") return indent + "NULL -> " + slot + ";\n";
		if (e.kind == "row") {
			const ElemOps& l = *e.leaf;
			string s = indent + "__pln_malloc(" + names[0] + " * " + sizeOf(l, rest(names, 1)) + ") -> " + slot + ";\n";
			if (l.built) s += indent + "__pln_build_" + l.key + "(" + slot + ", 0, " + names[0] + lenArgs(names, 1) + ");\n";
			return s;
		}
		return structElemBuild(e.type, slot, indent);
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// A row moved out by `->>` leaves NULL in its slot.
	auto elemDrop = [&](const ElemOps& e, const string& slot, const string& indent, const vector<string>& names) {
		if (e.kind == "embed-varr") return "__pln_fini_" + e.inner + "(@!" + slot + lenArgs(names) + ");";
		if (e.kind == "embed-row") return "__pln_drop_" + e.key + "(" + slot + " as " + e.data + ", 0, 1" + lenArgs(names) + ");";
		if (e.kind == "varr" || e.kind == "dict") return "__pln_free_" + e.inner + "(" + slot + lenArgs(names) + ");";
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
		if (e.kind == "dict")
			return "__pln_copy_" + e.inner + "(" + dst + ", " + src + lenArgs(names) + ");";
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
		return e.kind == "struct" || e.kind == "varr" || e.kind == "dict" ? (isMutable ? "@!" : "@") + v : v;
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
		return e.kind == "struct" || e.kind == "varr" || e.kind == "dict" || e.kind == "row";
	};
	// The body of a run's copy loop: a NULL source element leaves the slot
	// NULL, and a NULL slot is built before it is copied into whole.
	auto keepingNull = [&](const ElemOps& e, const string& copy, const string& ma) {
		string set = "__pln_set_" + e.key + "(dst, i, " + borrowed(e, "src[i]") + ma + ");";
		if (!slotMayBeNull(e)) return "        " + copy + "\n";
		string slots = "dst as " + e.data;
		string build = "__pln_build_" + e.key + "(" + slots + ", i, i + 1" + ma + ");";
		string fill = copy == set ? "            if dst[i] == NULL { " + build + " }\n            " + set + "\n"
		            : "            if dst[i] == NULL { " + build + " " + set + " }\n            else { " + copy + " }\n"; // LCOV_EXCL_BR_LINE
		return "        if src[i] == NULL { __pln_clear_" + e.key + "(" + slots + ", i" + ma + "); }\n"
		       "        else {\n" + fill + "        }\n";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// As keepingNull, for a struct's field.
	auto fieldKeepingNull = [&](const ElemOps& e, const string& slot, const vector<string>& names, bool assign) {
		string dst = "dst." + slot, src = "src." + slot;
		string set = elemSet(e, dst, src, names);
		string into = assign ? elemAssign(e, dst, src, names) : set;
		if (!slotMayBeNull(e)) return "    " + into + "\n";
		string build = "            if " + dst + " == NULL {\n" + elemBuild(e, dst, "                ", names);
		return "    if " + src + " == NULL {\n"
		       "        " + elemDrop(e, dst, "        ", names) + "\n"
		       "        NULL -> " + dst + ";\n"
		       "    } else {\n" + build
		     + (into == set ? "            }\n            " + set + "\n" // LCOV_EXCL_BR_LINE
		                    : "                " + set + "\n            } else { " + into + " }\n") // LCOV_EXCL_BR_LINE
		     + "    }\n";
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
		// A removed dictionary value is cleared too, an opaque one included.
		if (slotMayBeNull(e) || e.kind == "opaque")
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
		if (shape.contains("variants")) {
			int64_t rest = shape["total-size"].get<int64_t>() - shape["tag-size"].get<int64_t>();
			out << "\nexport type " << name << " { uint32 __pln_tag;";
			if (rest > 0) out << " $[" << rest << "]uint8 __pln_pad0;";
			out << " };\n";
			continue;
		}
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

	// A union's functions act on the variant its tag names, as the struct it
	// is laid out as when it owns something, and as bytes otherwise.
	auto emitUnion = [&](const json& shape) {
		string name = shape["shape-name"];
		string size = to_string(shape["total-size"].get<int64_t>());
		size_t n = shape["variants"].size();
		auto eachOwning = [&](const string& tagOf, const function<string(const string&)>& stmt) {
			string s;
			for (size_t k = 0; k < n; k++)
				if (variantOwns(shape, k))
					s += "    if " + tagOf + ".__pln_tag == " + to_string(k) + " { " + stmt(variantTypeName(shape, k)) + " return; }\n";
			return s;
		}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		out << "\nexport func __pln_alloc_" << name << "() -> @!" << name << " p {\n"
		    << "    __pln_malloc(" << size << ") -> p;\n"
		    << "    __pln_init_" << name << "(p);\n"
		    << "}\n"
		    << "export func __pln_init_" << name << "(@!" << name << " p) {\n"
		    << "    0 -> p.__pln_tag;\n";
		if (variantOwns(shape, 0))
			out << "    __pln_init_" << variantTypeName(shape, 0) << "(p as @!" << variantTypeName(shape, 0) << ");\n";
		out << "}\n"
		    << "export func __pln_fini_" << name << "(@!" << name << " p) {\n"
		    << eachOwning("p", [](const string& v) { return "__pln_fini_" + v + "(p as @!" + v + ");"; })
		    << "}\n"
		    << "export func __pln_free_" << name << "(@!" << name << " p) {\n"
		    << "    if (p == NULL) { return; }\n"
		    << "    __pln_fini_" << name << "(p);\n"
		    << "    free(p);\n"
		    << "    return;\n"
		    << "}\n"
		    << "export func __pln_copy_" << name << "(@!" << name << " dst, @" << name << " src) {\n"
		    // A variant's copy reuses what dst's contents own, as a struct's does.
		    << "    if dst.__pln_tag != src.__pln_tag {\n"
		    << "        __pln_fini_" << name << "(dst);\n"
		    << "        memset(dst, 0, " << size << ");\n"
		    << "    }\n"
		    << eachOwning("src", [](const string& v) { return "__pln_copy_" + v + "(dst as @!" + v + ", src as @" + v + ");"; })
		    << "    memcpy(dst, src, " << size << ");\n"
		    << "}\n";
		// Into an existing value, as copy is, but a variant's own assign writes
		// the same variant. A struct laying a union out in place writes it so
		// whenever it is written part by part.
		if (!shape.contains("copy-func"))
			out << "export func __pln_assign_" << name << "(@!" << name << " dst, @" << name << " src) {\n"
			    << "    if dst.__pln_tag != src.__pln_tag {\n"
			    << "        __pln_copy_" << name << "(dst, src);\n"
			    << "        return;\n"
			    << "    }\n"
			    << eachOwning("src", [](const string& v) { return "__pln_assign_" + v + "(dst as @!" + v + ", src as @" + v + ");"; })
			    << "    memcpy(dst, src, " << size << ");\n"
			    << "}\n";
		// Made another variant, as a store to it does: its contents are built
		// for a copy into them, and left NULL for one given to them.
		out << "export func __pln_become_" << name << "(@!" << name << " p, uint32 k, bool build) {\n"
		    << "    if p.__pln_tag == k { return; }\n"
		    << "    __pln_fini_" << name << "(p);\n"
		    << "    memset(p, 0, " << size << ");\n"
		    << "    k -> p.__pln_tag;\n";
		for (size_t k = 0; k < n; k++)
			if (variantOwns(shape, k))
				out << "    if build && k == " << k << " { __pln_init_" << variantTypeName(shape, k)
				    << "(p as @!" << variantTypeName(shape, k) << "); }\n";
		out << "}\n";
		out << "export func __pln_initrun_" << name << "(@!" << name << " run, int64 from, int64 to) {\n"
		    << "    int64 i = from;\n"
		    << "    while i < to {\n"
		    << "        __pln_init_" << name << "(@!run[i]);\n"
		    << "        i + 1 -> i;\n"
		    << "    }\n"
		    << "}\n";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	// Struct allocator/free functions, for structs owning fields
	for (auto& shape : struct_shapes) {
		if (shape["shape-kind"] == "bytes" || shape["shape-kind"] == "opaque") continue;
		if (shape.contains("variants")) {
			emitUnion(shape);
			continue;
		}
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
		    << "    __pln_malloc(" << shape["total-size"].get<int64_t>() << ") -> p;\n"
		    << "    __pln_init_" << name << "(p);\n"
		    << "}\n";

		// A struct in a stack frame releases only what it owns (fini);
		// a heap one also frees its own block.
		out << "export func __pln_init_" << name << "(@!" << name << " p) {\n";
		for (auto& f : owned) out << elemBuild(f.e, "p." + f.slot, "    ", f.lens);
		out << forRuns(shape, [](const string& sub, const string& slot) { return "__pln_init_" + sub + "(@!p." + slot + ");"; });
		// After the owned fields are built: an initializer writes into them.
		if (shape.contains("init-func"))
			out << "    " << shape["init-func"].get<string>() << "(p);\n";
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
			out << fieldKeepingNull(f.e, f.slot, f.lens, false);
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
				out << fieldKeepingNull(f.e, f.slot, f.lens, true);
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

	// A dictionary's header is std/rt's, which keeps the keys; its values
	// are the `[+]T` at its end, a value at each key's position. A removed
	// key's value is released at once and its slots left NULL, so that
	// nothing is released twice. Its functions take the lengths the values'
	// do, alloc and has too, so that every one is called alike.
	auto emitDict = [&](const string& key, const string& values) {
		ElemOps e = opsOf(*varrElemDescs.at(values));
		vector<string> names = lenNames(e.nlens);
		string mp = lenParams(e.nlens), ma = lenArgs(names);
		string hdr = "__pln_" + key, vals = "__pln_" + values;
		out << "\nexport type " << hdr << " { int64 live; int64 used; int64 cap; int64 ents; int64 tsize; int64 table; $"
		    << vals << " values; };\n"
		    << "export func __pln_alloc_" << key << "(" << (mp.empty() ? "" : mp.substr(2)) << ") -> @!" << hdr << " h {\n"
		    << "    __pln_dict_new() -> h;\n"
		    << "}\n"
		    << "export func __pln_free_" << key << "(@!" << hdr << " h" << mp << ") {\n"
		    << "    if (h == NULL) { return; }\n"
		    << "    __pln_fini_" << values << "(@!h.values" << ma << ");\n"
		    << "    __pln_dict_free(h);\n"
		    << "    return;\n"
		    << "}\n"
		// The keys keep their positions, so the values are copied as one run.
		    << "export func __pln_copy_" << key << "(@!" << hdr << " dst, @" << hdr << " src" << mp << ") {\n"
		    << "    if (dst as @void == src as @void) { return; }\n"
		    << "    __pln_dict_copy(dst, src);\n"
		    << "    __pln_copy_" << values << "(@!dst.values, src.values.data, src.values.len" << ma << ");\n"
		    << "}\n";
		// `d[k]` is the value's address. An owned value's slot is a pointer,
		// which '@!' cannot address, so the address is reckoned as an integer.
		string size = sizeOf(e, names);
		auto valueAt = [&](const string& pos) {
			return "(int64(h.values.data) + " + pos + " * " + size + ") as @!void";
		}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		out << "export func __pln_at_" << key << "(@" << hdr << " h, @[]uint8 k" << mp << ") -> @!void {\n"
		    << "    return " << valueAt("__pln_dict_at(h, k)") << ";\n"
		    << "}\n"
		// A new key's position is the values' length.
		    << "export func __pln_put_" << key << "(@!" << hdr << " h, @[]uint8 k" << mp << ") -> @!void {\n"
		    << "    int64 pos = __pln_dict_add(h, k);\n"
		    << "    if pos == h.values.len { __pln_resize_" << values << "(@!h.values, pos + 1" << ma << "); }\n"
		    << "    return " << valueAt("pos") << ";\n"
		    << "}\n"
		    << "export func __pln_has_" << key << "(@" << hdr << " h, @[]uint8 k" << mp << ") -> bool {\n"
		    << "    return __pln_dict_find(h, k) >= 0;\n"
		    << "}\n"
		    << "export func __pln_remove_" << key << "(@!" << hdr << " h, @[]uint8 k" << mp << ") -> bool {\n"
		    << "    int64 pos = __pln_dict_remove(h, k);\n"
		    << "    if pos < 0 { return false; }\n";
		if (e.kind == "embed-row" && e.leaf->owned) {
			string n = countOf(e, names);
			out << "    int64 j = 0;\n"
			    << "    while j < " << n << " {\n"
			    << "        __pln_clear_" << e.leaf->key << "(h.values.data, pos * " << n << " + j" << lenArgs(names, e.depth) << ");\n"
			    << "        j + 1 -> j;\n"
			    << "    }\n";
		} else if (e.owned) {
			out << "    __pln_clear_" << e.key << "(h.values.data, pos" << ma << ");\n";
		}
		out << "    __pln_dict_settle(h, " << size << ");\n"
		    << "    return true;\n"
		    << "}\n"
		    << "export func __pln_clear_" << key << "(@!" << hdr << " h" << mp << ") {\n"
		    << "    __pln_resize_" << values << "(@!h.values, 0" << ma << ");\n"
		    << "    __pln_dict_clear(h);\n"
		    << "}\n";
	}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	for (auto& shape : elem_arr_shapes) {
		string key = shape["shape-key"];
		if (shape["shape-kind"] == "dict") {
			emitDict(key, shape["values"]);
			continue;
		}
		ElemOps e = opsOf(shape["elem"]);
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
		    << "    if cap > 0 { __pln_malloc(cap * " << size << ") -> h.data; }\n"
		    << "    else { NULL -> h.data; }\n";
		if (e.built) out << "    __pln_build_" << e.key << "(h.data, 0, len" << ma << ");\n";
		out << "}\n"
		// The header is returned as its block, as a struct allocator's is.
		    << "export func __pln_alloc_" << key
		    << "(int64 len, int64 cap" << mp << ") -> @!" << hdr << " h {\n"
		    << "    __pln_malloc(24) -> h;\n"
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
		    << "    __pln_realloc(h.data, c * " << size << ") -> h.data;\n"
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
			out << "    x" << (e.kind == "row" ? " as " + e.popped : "") << " -> h.data[h.len];\n";
		out << "    h.len + 1 -> h.len;\n"
		    << "}\n"
		    << "export func __pln_resize_" << key << "(@!" << hdr << " h, int64 n" << mp << ") {\n"
		    << (checks ? "    if n < 0 { __pln_negative_size(); }\n" : "")
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
			    << (checks ? "    if h.len == 0 { __pln_pop_empty(); }\n" : "")
			    << "    h.len - 1 -> h.len;\n"
			    << "    " << e.type << " x <<= h.data[h.len] as " << e.type << ";\n"
			    << "    return x;\n"
			    << "}\n";
			continue;
		}
		// An owned element leaves its slot for the caller.
		out << "export func __pln_pop_" << key << "(@!" << hdr << " h" << mp << ") -> " << e.popped << " x {\n"
		    << (checks ? "    if h.len == 0 { __pln_pop_empty(); }\n" : "")
		    << "    h.len - 1 -> h.len;\n";
		if (e.kind == "embed-row")
			out << "    __pln_malloc(" << size << ") -> x;\n";
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
} // LCOV_EXCL_BR_LINE
