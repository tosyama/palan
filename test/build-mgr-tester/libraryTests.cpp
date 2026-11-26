/// palan build manager tests: using a library through its interface and archive
///
/// @file libraryTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "../test-base/testBase.h"
#include "../../src/common/PlnDefs.h"
#include "buildMgrTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

static const char VEC_SOURCE[] = R"(library geo;
cinclude <math.h> link "m";
export type Vec { flo64 x; flo64 y; };
export type Poly { [2]Vec pts; [+]int32 tags; int32 n; };
export func vec(flo64 x, flo64 y) -> Vec v { x -> v.x; y -> v.y; }
export func +(@Vec a, @Vec b) -> Vec v { a.x + b.x -> v.x; a.y + b.y -> v.y; }
export func <<(@!Vec a, @Vec b) -> @!Vec { a.x + b.x -> a.x; a.y + b.y -> a.y; return a; }
export func norm(@Vec a) -> flo64 { return sqrt(a.x * a.x + a.y * a.y); }
export func makePoly(int32 n) -> Poly p { n -> p.n; p.tags.resize(3); }
export func tagCount(@Poly p) -> int64 { return len(@p.tags); }
)";

static const char SHAPE_SOURCE[] = R"(library draw;
libpath "../libs";
import <geo/vec>;
export type Shape { Vec c; Poly p; };
export func size(@Shape s) -> flo64 { return norm(@s.c); }
)";

static string outAbsolute(const string& path) {
	return fs::weakly_canonical("out/" + path).string();
}

static void writeFile(const string& path, const string& text) {
	fs::create_directories(fs::path(path).parent_path());
	ofstream(path) << text;
}

static string fileText(const string& path) {
	ifstream f(path);
	stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// The sources are removed, so what the program builds from is only what a library ships.
static void buildGeoLibrary() {
	cleanTestEnv();
	writeFile("out/src/vec.pa", VEC_SOURCE);
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/vec.pa"), "");
	fs::remove_all("out/src");
}

static void buildDrawLibrary() {
	writeFile("out/src/shape.pa", SHAPE_SOURCE);
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/shape.pa"), "");
	fs::remove_all("out/src");
}

TEST(build_mgr, library_used_without_its_source) {
	buildGeoLibrary();
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <geo/vec>;
cinclude <stdio.h>;
cinclude <mcheck.h>;
{
	mtrace();
	Vec a = vec(3.0, 4.0);
	Vec c = a + vec(1.0, 1.0);
	c << vec(1.0, 1.0);
	printf("%.1f %.1f %.1f\n", c.x, c.y, norm(@a));
	Poly p = makePoly(5);
	printf("%d %ld\n", p.n, tagCount(@p));
	[2]Poly ps;
	printf("%ld\n", tagCount(@ps[1]));
}
)");
	ASSERT_EQ(execTestCommand("bin/palan -o out/p out/app/main.pa"), "");
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so MALLOC_TRACE=out/t.log out/p");
	ASSERT_EQ(output, "5.0 6.0 5.0\n5 3\n0\n");
	// The same program importing the library's source allocates just as many.
	expectBalancedMtrace("out/t.log", 19);
}

TEST(build_mgr, library_import_forms) {
	buildGeoLibrary();
	writeFile("out/app/main.pa", R"(libpath "../libs";
import vec, norm, Vec from <geo/vec>;
import <geo/vec> as G;
cinclude <stdio.h>;
Vec a = vec(3.0, 4.0);
G.Vec b = G.vec(6.0, 8.0);
printf("%.1f %.1f\n", norm(@a), G.norm(@b));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0 10.0\n");
}

// The library's main initializes its internal module first, through the
// import, so the program needs only what it imports.
TEST(build_mgr, library_module_globals) {
	cleanTestEnv();
	writeFile("out/src/count.pa", R"(library kit;
import "inner.pa";
global int32 base = innerStart() + 1 for next!;
export func next() -> int32 { base + 1 -> base; return base; }
)");
	writeFile("out/src/inner.pa", R"(library kit;
cinclude <stdio.h>;
func start() -> int32 { puts("inner"); return 10; }
global int32 seed = start() for innerStart;
export func innerStart() -> int32 { return seed; }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/count.pa"), "");
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/count>;
cinclude <stdio.h>;
printf("%d\n", next());
printf("%d\n", next());
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "inner\n12\n13\n");
}

// Only the main's exported globals are published; the program uses them
// from the interface and the archive alone.
TEST(build_mgr, library_export_global) {
	cleanTestEnv();
	writeFile("out/src/m.pa", R"(library kit;
import "inner.pa";
export global int64 hits = 0 for hit!;
export global []int32 primes = [2, 3, 5];
global secret for peek;
export func hit() { hits + 1 -> hits; }
export func peek() -> int64 { return secret; }
)");
	writeFile("out/src/inner.pa", R"(library kit;
export global int64 secret = 7;
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/m.pa"), "");
	string pai = fileText("out/libs/kit/m.pai");
	ASSERT_NE(pai.find("export global int64 hits;\nexport global [3]int32 primes;\n"), string::npos) << pai;
	ASSERT_EQ(pai.find("secret"), string::npos) << pai;
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/m>;
import <kit/m> as K;
cinclude <stdio.h>;
global hits for f;
func f() -> int64 { return hits; }
hit();
K.hits + 10 -> K.hits;
printf("%ld %ld %d %d %ld\n", hits, f(), primes[2], K.primes[0], peek());
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "11 11 5 2 7\n");
}

// The program reads the defaults from the interface alone, through an alias too.
TEST(build_mgr, library_parameter_defaults) {
	cleanTestEnv();
	writeFile("out/src/opt.pa", R"(library kit;
cinclude <stdio.h>;
export type Mode enum { Fast, Slow };
const LIMIT = 40;
export func run(int32 a, int32 lim = LIMIT + 2, Mode m = Mode.Slow, @[]uint8 tag = "dflt") {
	printf("%d %d %d %s\n", a, lim, m, tag);
}
const MASK = 0xFFFF0000;
export func bits(int32 a = 0xFFFFFFFE, int32 b = MASK) { printf("%d %d\n", a, b); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/opt.pa"), "");
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/opt> as K;
K.run(1);
K.run(2, 7, K.Mode.Fast);
K.bits();
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "1 42 1 dflt\n2 7 0 dflt\n-2 -65536\n");
}

TEST(build_mgr, library_recursive_type) {
	cleanTestEnv();
	writeFile("out/src/tree.pa", R"(library kit;
export type Tree { int32 v; [+]Tree kids; [""]Link links; };
export type Link { Tree to; };
export func first(@Tree t) -> int32 { return t.kids[0].v; }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/tree.pa"), "");
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/tree> as K;
cinclude <stdio.h>;
K.Tree t; K.Tree k; K.Link l;
4 -> k.v;
t.kids.push(k);
9 -> l.to.v;
l -> t.links["x"];
K.Tree c = t;
printf("%d %d\n", K.first(@c), c.links["x"].to.v);
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "4 9\n");
}

// A union crosses the interface with its variants, and as an opaque type's contents.
TEST(build_mgr, library_union) {
	cleanTestEnv();
	writeFile("out/src/val.pa", R"(library kit;
export type Val union { Null; int64 i; [+]Val items; Box box; };
export type Box { int32 v; };
type Inner union { [+]int32 xs; Null; };
export type Hidden = opaque Inner;
export func make() -> Hidden { Inner x; return x as Hidden; }
export func size(@Val v) -> int64 { return sizeof(Val); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	string pai = execTestCommand("cat out/libs/kit/val.pai");
	EXPECT_NE(pai.find("export type Val union {\n\tNull;\n\tint64 i;\n\t[+]Val items;\n\tBox box;\n};"), string::npos) << pai;
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/val> as K;
cinclude <stdio.h>;
K.Val v;
K.Val c = v;
[+]K.Val vs = [];
vs.push(c);
K.Hidden h = K.make();
printf("%ld %ld\n", K.size(@vs[0]), len(@vs));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "16 1\n");
}

// A variadic parameter crosses the interface as written, also inside a
// function pointer type, and is called through an alias.
TEST(build_mgr, library_variadic) {
	cleanTestEnv();
	writeFile("out/src/va.pa", R"(library kit;
cinclude <stdio.h>;
export type P { int32 x; };
export func join(@[]uint8 sep, [...]@[]uint8 parts) {
	for copy s, i in parts { if i > 0 { printf("%s", sep); } printf("%s", s); }
	printf("\n");
}
export func sum([...]P ps) -> int32 t { for @p in ps { t + p.x -> t; } }
export func pick(@func(int32, [...]int32) -> int32 f) -> int32 { return f(1, 2, 3); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/va.pa"), "");
	fs::remove_all("out/src");
	string pai = fileText("out/libs/kit/va.pai");
	EXPECT_NE(pai.find("export func join(@[]uint8 sep, [...]@[]uint8 parts);\n"), string::npos) << pai;
	EXPECT_NE(pai.find("export func pick(@func(int32, [...]int32) -> int32 f) -> int32;\n"), string::npos) << pai;
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/va> as K;
cinclude <stdio.h>;
func add(int32 a, [...]int32 xs) -> int32 t { a -> t; for copy x in xs { t + x -> t; } }
K.join(", ", "a", "b", "c");
K.P p = {x: 2};
printf("%d %d\n", K.sum(p, [3]), K.pick(add));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "a, b, c\n5 6\n");
}

// A type's '->' crosses the interface with its type, a private one's too.
TEST(build_mgr, library_copy_func) {
	cleanTestEnv();
	writeFile("out/src/tx.pa", R"(library kit;
cinclude <stdio.h>;
type Inner { [+]int32 v; int32 keep; };
func ->(@Inner src, @!Inner dst) { src.v -> dst.v; printf("I%d ", dst.keep); }
export type T { int32 mode; Inner in; };
func ->(@T src, @!T dst) { src.in -> dst.in; printf("T%d ", dst.mode); }
export func mk(int32 m) -> T t { m -> t.mode; m -> t.in.keep; }
type Unused { int32 x; };
func ->(@Unused src, @!Unused dst) { }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/tx.pa"), "");
	fs::remove_all("out/src");
	string pai = fileText("out/libs/kit/tx.pai");
	EXPECT_EQ(pai.find("Unused"), string::npos) << pai;
	EXPECT_NE(pai.find("export func mk(int32 m) -> T t;\nfunc ->(@Inner src, @!Inner dst);\nfunc ->(@T src, @!T dst);\n"),
		string::npos) << pai;
	writeFile("out/app/main.pa", R"(libpath "../libs";
cinclude <stdio.h>;
import <kit/tx>;
T a = mk(1); T b = mk(2);
a -> b;
[+]T vs; vs.push(mk(3));
[+]T ws; ws.push(mk(4)); ws.push(mk(5));
vs -> ws;
printf("%d %d %ld\n", b.mode, ws[0].in.keep, len(@ws));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "I2 T2 I4 T4 2 4 1\n");
}

// A library hides an internal module's type behind an opaque one: users own,
// move and hold it, and release it through the library, with or without a
// function marked `release`.
TEST(build_mgr, library_opaque_type) {
	cleanTestEnv();
	writeFile("out/src/conn.pa", R"(library net;
export type Conn { int32 fd; [+]int32 log; };
export func touch(@!Conn c, int32 fd) { fd -> c.fd; c.log.push(fd); }
)");
	writeFile("out/src/sock.pa", R"(library net;
cinclude <stdio.h>;
import "conn.pa" as C;
export type Sock = opaque C.Conn;
export type Buf = opaque [+]int32;
export func open(int32 fd) -> Sock {
	C.Conn c;
	C.touch(@!c, fd);
	return c as Sock;
}
export func fd(@Sock s) -> int32 { return (s as @C.Conn).fd; }
export func close(Sock s) release { printf("close %d\n", fd(@s)); }
export func buf(int32 n) -> Buf {
	[+]int32 v;
	v.resize(n);
	return v as Buf;
}
export func size(@Buf b) -> int64 { return len(b as @[+]int32); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/sock.pa"), "");
	fs::remove_all("out/src");
	string pai = fileText("out/libs/net/sock.pai");
	EXPECT_EQ(pai.find("Conn"), string::npos) << pai;
	EXPECT_NE(pai.find("export type Sock = opaque;\nexport type Buf = opaque;\n"), string::npos) << pai;
	EXPECT_NE(pai.find("export func close(Sock s) release;\n"), string::npos) << pai;

	writeFile("out/app/main.pa", R"(libpath "../libs";
import <net/sock>;
cinclude <stdio.h>;
cinclude <mcheck.h>;
type Peer { Sock s; int32 id; };
{
	mtrace();
	Sock a = open(1);
	Sock b <<= a;
	close(b>>);
	Peer p;
	open(2) ->> p.s;
	[3]Sock ss;
	open(3) ->> ss[1];
	[+]Sock vs;
	vs.push(open(4));
	Buf f = buf(5);
	printf("%d %ld
", fd(@p.s), size(@f));
}
)");
	ASSERT_EQ(execTestCommand("bin/palan -o out/p out/app/main.pa"), "");
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so MALLOC_TRACE=out/t.log out/p");
	ASSERT_EQ(output, "close 1\n2 5\nclose 4\nclose 3\nclose 2\n");
	expectBalancedMtrace("out/t.log", 18);

	writeFile("out/app/copy.pa", "libpath \"../libs\";\nimport <net/sock>;\nSock a = open(1);\nSock b = a;\n");
	ASSERT_NE(execTestCommand("bin/palan -o out/c out/app/copy.pa").find(
		"copy.pa:4:1: error: a copy needs the contents of the opaque type 'Sock'"), string::npos);
}

// Two libraries each export a Point, with its own layout, allocators and '->'.
TEST(build_mgr, libraries_exporting_one_type_name) {
	cleanTestEnv();
	writeFile("out/src/a/pt.pa", R"(library ka;
export type Point { int32 x; [+]int32 tags; };
func ->(@Point src, @!Point dst) { src.x + 100 -> dst.x; src.tags -> dst.tags; }
export func mk(int32 x) -> Point p { x -> p.x; p.tags.push(x); }
export func show(@Point p) -> int32 { return p.x + p.tags[0]; }
)");
	writeFile("out/src/b/pt.pa", R"(library kb;
export type Point { flo64 y; int64 n; };
export func mk(flo64 y) -> Point p { y -> p.y; 2 -> p.n; }
export func show(@Point p) -> int64 { return p.n; }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/a/pt.pa"), "");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/b/pt.pa"), "");
	writeFile("out/app/main.pa", R"(libpath "../libs";
cinclude <stdio.h>;
import <ka/pt>;
import <kb/pt> as B;
Point a = mk(1);
Point c = mk(5);
a -> c;
[+]B.Point bs;
bs.push(B.mk(1.5));
printf("%d %ld %.1f\n", show(@c), B.show(@bs[0]), bs[0].y);
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "102 2 1.5\n");
}

// The interface keeps hidden fields, which the user's allocators lay out and copy.
TEST(build_mgr, library_hidden_fields) {
	cleanTestEnv();
	writeFile("out/src/bag.pa", R"(library kit;
export type Bag {
	private [+]int32 items;
	readonly int32 count = 0;
	int32 tag;
};
export func add(@!Bag b, int32 v) -> @!Bag { b.items.push(v); b.count + 1 -> b.count; return b; }
export func sum(@Bag b) -> int32 s { for copy v in b.items { s + v -> s; } }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/bag.pa"), "");
	fs::remove_all("out/src");
	string pai = fileText("out/libs/kit/bag.pai");
	EXPECT_NE(pai.find("\tprivate [+]int32 items;\n\treadonly int32 count = 0;\n\tint32 tag;\n"), string::npos) << pai;
	writeFile("out/app/main.pa", R"(libpath "../libs";
cinclude <stdio.h>;
import <kit/bag>;
Bag a; 9 -> a.tag;
a.add(1).add(2);
Bag b = a;
b.add(4);
Bag c; b -> c;
printf("%d %d %d %d %d\n", sum(@a), a.count, sum(@c), c.count, c.tag);
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "3 2 7 3 9\n");
	writeFile("out/app/bad.pa", "libpath \"../libs\";\nimport <kit/bag>;\nBag a;\n3 -> a.count;\n");
	EXPECT_NE(execTestCommand("bin/palan out/app/bad.pa").find("the field 'count' of 'Bag' is read-only"),
		string::npos);
}

// A libpath adds a place for the whole program, wherever it is written and relative to that file.
TEST(build_mgr, libpath_in_an_imported_module) {
	buildGeoLibrary();
	writeFile("out/app/sub/helper.pa", "libpath \"../../libs\";\n");
	writeFile("out/app/main.pa", R"(import "sub/helper.pa";
import <geo/vec>;
cinclude <stdio.h>;
Vec a = vec(3.0, 4.0);
printf("%.1f\n", norm(@a));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0\n");
}

// The object a `cinclude` links goes into the archive, since its source does not ship.
TEST(build_mgr, library_carries_a_linked_object) {
	cleanTestEnv();
	string c = "../test/testdata/build-mgr/295_link_obj";
	ASSERT_EQ(execTestCommand("mkdir -p out/src && cp " + c + ".h out/src/ && gcc -c -O1 " + c + ".c -o out/src/295_link_obj.o"), "");
	writeFile("out/src/sum.pa", R"(library calc;
cinclude "295_link_obj.h" link "295_link_obj.o";
export func sum3() -> int32 { return add3(1, 2, 3); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/sum.pa"), "");
	fs::remove_all("out/src");
	EXPECT_EQ(execTestCommand("ar t out/libs/calc/sum.a"), "calc.sum.o\n__obj0_295_link_obj.o\n");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <calc/sum>;
cinclude <stdio.h>;
printf("%d\n", sum3());
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "6\n");
}

TEST(build_mgr, library_c_export) {
	cleanTestEnv();
	writeFile("out/src/cb.h", "#include <stdint.h>\nint32_t quad(int32_t v);\n");
	writeFile("out/src/cb.c", "#include \"cb.h\"\nint32_t twice(int32_t v);\nint32_t quad(int32_t v) { return twice(twice(v)); }\n");
	ASSERT_EQ(execTestCommand("gcc -c -O1 out/src/cb.c -o out/src/cb.o"), "");
	writeFile("out/src/twice.pa", R"(library calc;
cinclude "cb.h" link "cb.o";
cexport func twice(int32 v) -> int32 { return v * 2; }
export func quad4() -> int32 { return quad(4); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/twice.pa"), "");
	string pai = fileText("out/libs/calc/twice.pai");
	EXPECT_NE(pai.find("cexport func twice(int32 v) -> int32;\n"), string::npos) << pai;
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <calc/twice>;
cinclude <stdio.h>;
printf("%d %d\n", twice(5), quad4());
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "10 16\n");
}

TEST(build_mgr, library_using_a_library) {
	buildGeoLibrary();
	buildDrawLibrary();
	string pai = fileText("out/libs/draw/shape.pai");
	EXPECT_NE(pai.find("import <geo/vec>;\n"), string::npos) << pai;
	EXPECT_EQ(pai.find("libpath"), string::npos) << pai;

	writeFile("out/app/main.pa", R"(libpath "../libs";
import <draw/shape>;
cinclude <stdio.h>;
Shape s;
3.0 -> s.c.x;
4.0 -> s.c.y;
printf("%.1f\n", size(@s));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0\n");
}

TEST(build_mgr, library_changes_reach_its_users) {
	cleanTestEnv();
	writeFile("out/src/val.pa", "library lib;\nexport func val(int32 a) -> int32 { return a; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <lib/val>;
cinclude <stdio.h>;
printf("%d\n", val(1));
)");
	ASSERT_EQ(execTestCommand("bin/palan out/app/main.pa"), "1\n");

	writeFile("out/src/val.pa", "library lib;\nexport func val(int32 a) -> int32 { return a + 1; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "2\n");

	// A parameter's type is in the symbol, so an importer not compiled again fails to link.
	writeFile("out/src/val.pa", "library lib;\nexport func val(int64 a) -> int64 { return a + 2; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "3\n");
}

TEST(build_mgr, library_lookup_errors) {
	buildGeoLibrary();
	string main_pa = outAbsolute("app/main.pa");

	writeFile(main_pa, "import <geo/none>;\nlibpath \"../libs\";\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: '" + main_pa + "' imports <geo/none>, which no libpath has\n");

	writeFile(main_pa, "import <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: '" + main_pa + "' imports <geo/vec>, which no libpath has\n");

	// Each libpath alone is enough for the import, and order must not decide between them.
	fs::create_directories("out/other/geo");
	writeFile(main_pa, "libpath \"../libs\";\nlibpath \"../other\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: library 'geo' is found in both '" + outAbsolute("libs") + "' and '" + outAbsolute("other") + "'\n");

	// One directory written twice is not two places.
	writeFile(main_pa, "libpath \"../libs\";\nlibpath \"../libs/\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa), "");
}

TEST(build_mgr, library_dependency_needs_the_programs_libpath) {
	buildGeoLibrary();
	buildDrawLibrary();
	fs::create_directories("out/libs2");
	fs::rename("out/libs/draw", "out/libs2/draw");
	writeFile("out/app/main.pa", "libpath \"../libs2\";\nimport <draw/shape>;\n");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs2/draw/shape.pai") + "' imports <geo/vec>, which no libpath has\n");
}

TEST(build_mgr, library_interface_errors) {
	buildGeoLibrary();
	string pai = "out/libs/geo/vec.pai";
	string text = fileText(pai);
	writeFile("out/app/main.pa", "libpath \"../libs\";\nimport <geo/vec>;\n");

	auto replaced = [&](const string& from, const string& to) {
		string edited = text;
		edited.replace(edited.find(from), from.size(), to);
		return edited;
	};
	writeFile(pai, replaced("built \"" PALAN_VERSION "\"", "built \"0.0.1\""));
	EXPECT_NE(execTestCommand("bin/palan out/app/main.pa").find("built by version 0.0.1"), string::npos);
	writeFile(pai, replaced("target \"" PALAN_TARGET "\"", "target \"aarch64-linux-gnu\""));
	EXPECT_NE(execTestCommand("bin/palan out/app/main.pa").find("is for the target aarch64-linux-gnu"), string::npos);
	writeFile(pai, text);
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "");

	fs::remove("out/libs/geo/vec.a");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs/geo/vec.pai") + "' has no archive beside it\n");
}

TEST(build_mgr, library_identity_met_by_a_source_module) {
	buildGeoLibrary();
	string source = outAbsolute("app/vec.pa");
	writeFile(source, "library geo;\n");
	writeFile("out/app/main.pa", "libpath \"../libs\";\nimport \"vec.pa\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs/geo/vec.pai") + "' has the same module identity 'geo.vec' as '" + source + "'\n");
}
