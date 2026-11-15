/// codegen tests
///
/// @file basicTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include "../test-base/testBase.h"
#include "../../src/codegen/PlnVProg.h"
#include "../../src/codegen/PlnRegAlloc.h"

using namespace std;

static string run_codegen(const string& sa_file, const string& asm_out)
{
    return execTestCommand("palan-codegen " + sa_file + " -o " + asm_out);
}

static string readFile(const string& path)
{
    ifstream f(path);
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static size_t countOccurrences(const string& text, const string& sub)
{
    size_t n = 0;
    for (size_t p = text.find(sub); p != string::npos; p = text.find(sub, p + sub.size()))
        n++;
    return n;
}

TEST(codegen, printf_int_literal) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/002_printf_int_literal.sa.json";
    string asmf = "out/002_printf_int_literal.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq $42,"),    string::npos);
    ASSERT_NE(asm_text.find("%rdi"),         string::npos);
    ASSERT_NE(asm_text.find("%rsi"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

// Verify that two sequential C calls each get correct argument registers.
// This exercises the multi-call path with the vector<VReg> CallC design.
// (Callee-saved register allocation is tested in the variable declaration tests.)
TEST(codegen, two_calls_arg_registers) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/003_two_calls.sa.json";
    string asmf = "out/003_two_calls.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // puts call: only %rdi
    ASSERT_NE(asm_text.find("call puts"),    string::npos);
    // printf call: %rdi (format) and %rsi (integer 99)
    ASSERT_NE(asm_text.find("movq $99,"),    string::npos);
    ASSERT_NE(asm_text.find("%rsi"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, var_decl_init) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/004_var_decl.sa.json";
    string asmf = "out/004_var_decl.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // x is initialized with 10 and passed to printf
    ASSERT_NE(asm_text.find("movq $10,"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"),     string::npos);
}

TEST(codegen, addition) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/005_addition.sa.json";
    string asmf = "out/005_addition.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq"),        string::npos);
    ASSERT_NE(asm_text.find("addq"),        string::npos);
    ASSERT_NE(asm_text.find("subq"),        string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, comparison) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/018_comparison.sa.json";
    string asmf = "out/018_comparison.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("cmpq"),        string::npos);
    ASSERT_NE(asm_text.find("setl"),        string::npos);
    ASSERT_NE(asm_text.find("sete"),        string::npos);
    ASSERT_NE(asm_text.find("movzbl"),      string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, convert_int32_to_int64) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/006_convert.sa.json";
    string asmf = "out/006_convert.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movslq"), string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, var_decl_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/007_var_decl_int32.sa.json";
    string asmf = "out/007_var_decl_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int32 init must use movl, not movq
    ASSERT_NE(asm_text.find("movl $10,"),    string::npos);
    ASSERT_EQ(asm_text.find("movq $10,"),    string::npos);
    // int32 arg to printf must use 32-bit register (%esi), not 64-bit (%rsi)
    ASSERT_NE(asm_text.find(", %esi\n"),     string::npos);
    ASSERT_EQ(asm_text.find(", %rsi\n"),     string::npos);
    ASSERT_EQ(asm_text.find("movq -"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, addition_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/008_addition_int32.sa.json";
    string asmf = "out/008_addition_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int32 addition must use movl/addl, not movq/addq
    ASSERT_NE(asm_text.find("movl"),         string::npos);
    ASSERT_NE(asm_text.find("addl"),         string::npos);
    ASSERT_EQ(asm_text.find("addq"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, uint_lit) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/010_uint_lit.sa.json";
    string asmf = "out/010_uint_lit.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq $42,"), string::npos);  // lit-uint emits Int64 movq
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, ccall_as_expr) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/011_ccall_as_expr.sa.json";
    string asmf = "out/011_ccall_as_expr.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call abs"),    string::npos);  // inner CCCall emitted
    ASSERT_NE(asm_text.find("call printf"), string::npos);  // outer call emitted
}

TEST(codegen, cast_narrowing_int64_to_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/009_cast_narrowing.sa.json";
    string asmf = "out/009_cast_narrowing.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movl "), string::npos);   // narrowing uses movl
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, callpln_retpln_regalloc) {
    static const PhysRegs testPhysRegs = {
        {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"},  // intArgs
        {},                                                // floatArgs
        {"%rbx", "%r12", "%r13", "%r14", "%r15"},         // calleeSaved
    };

    // Simulate: caller calls add(v0=1, v1=2) -> v2, then returns v2
    VFunc f;
    f.name    = "caller";
    f.instrs.push_back(MovImm{0, VRegType::Int32, 1});
    f.instrs.push_back(MovImm{1, VRegType::Int32, 2});
    f.instrs.push_back(CallPln{"add", {0, 1}, {2}, {VRegType::Int32}});
    f.instrs.push_back(RetPln{{2}, {VRegType::Int32}});

    RegAllocResult ra = allocateRegisters(f, testPhysRegs);
    const RegMap& rm  = ra.regMap;

    // v0 and v1 should be assigned to arg registers (rdi, rsi)
    ASSERT_TRUE(rm.count(0));
    EXPECT_EQ(rm.at(0).base, "%rdi");
    ASSERT_TRUE(rm.count(1));
    EXPECT_EQ(rm.at(1).base, "%rsi");

    // v2 is defined by CallPln and used by RetPln — must be allocated
    ASSERT_TRUE(rm.count(2));

    // frame size for non-entry must be a multiple of 16
    EXPECT_EQ(ra.frameSize % 16, 0);
}

TEST(codegen, palan_func_simple) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/012_palan_func_simple.sa.json";
    string asmf = "out/012_palan_func_simple.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("add:"),        string::npos);  // function label emitted
    ASSERT_NE(asm_text.find("call add"),    string::npos);  // caller invokes add
    ASSERT_NE(asm_text.find("ret"),         string::npos);  // ret instruction present
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, assign_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/013_assign.sa.json";
    string asmf = "out/013_assign.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("passthrough:"),  string::npos);  // function label emitted
    ASSERT_NE(asm_text.find("call passthrough"), string::npos);  // caller invokes passthrough
    ASSERT_NE(asm_text.find("movl $0,"),      string::npos);  // dead InitVar for y still emitted
    ASSERT_NE(asm_text.find("ret"),           string::npos);
    ASSERT_NE(asm_text.find("call printf"),   string::npos);
}

// Regression: named return with narrowing (Int64 -> Int32) must insert a Convert in
// sa_assign_stmt, and callee-saved registers used inside the function must be saved and
// restored in the prologue/epilogue.
TEST(codegen, named_ret_narrowing) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/014_named_ret_narrowing.sa.json";
    string asmf = "out/014_named_ret_narrowing.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("test:"),          string::npos);  // function label
    ASSERT_NE(asm_text.find("call test"),       string::npos);  // caller invokes test
    // Callee-saved register %rbx must be saved in prologue and restored before ret
    ASSERT_NE(asm_text.find("movq %rbx, -8(%rbp)"),   string::npos);  // save
    ASSERT_NE(asm_text.find("movq -8(%rbp), %rbx"),   string::npos);  // restore
    ASSERT_NE(asm_text.find("ret"),            string::npos);
}

// Regression: when callee-saved registers are exhausted by temporaries, an Add result is
// spilled to a stack slot; X86CodeGen must emit a memory-destination Add instruction.
TEST(codegen, named_ret_double_assign) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/015_named_ret_double_assign.sa.json";
    string asmf = "out/015_named_ret_double_assign.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("test:"),        string::npos);
    ASSERT_NE(asm_text.find("call test"),    string::npos);
    // All 5 callee-saved registers must be saved/restored
    ASSERT_NE(asm_text.find("movq %rbx, -8(%rbp)"),   string::npos);
    ASSERT_NE(asm_text.find("movq -8(%rbp), %rbx"),   string::npos);
    ASSERT_NE(asm_text.find("movq %r15, -40(%rbp)"),  string::npos);
    ASSERT_NE(asm_text.find("movq -40(%rbp), %r15"),  string::npos);
    // Add with spilled dst: routed through scratch %rax to avoid mem-mem.
    ASSERT_NE(asm_text.find("addq %r15, %rax"),       string::npos);
    ASSERT_NE(asm_text.find("movq %rax, -48(%rbp)"),  string::npos);
    ASSERT_NE(asm_text.find("ret"),          string::npos);
}

TEST(codegen, multiret) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/016_multiret.sa.json";
    string asmf = "out/016_multiret.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("sumsOf:"),       string::npos);  // function label
    ASSERT_NE(asm_text.find("call sumsOf"),   string::npos);  // caller invokes sumsOf
    ASSERT_NE(asm_text.find("ret"),           string::npos);  // ret instruction
    ASSERT_NE(asm_text.find("call printf"),   string::npos);
}

TEST(codegen, helloworld_asm_output) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/001_helloworld.sa.json";
    string asmf = "out/001_helloworld.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find(".rodata"),        string::npos);
    ASSERT_NE(asm_text.find(".str0"),          string::npos);
    ASSERT_NE(asm_text.find("Hello World!\\n"), string::npos);
    ASSERT_NE(asm_text.find(".text"),          string::npos);
    ASSERT_NE(asm_text.find("\t.globl helloworld..main\nhelloworld..main:\n"), string::npos);
    ASSERT_NE(asm_text.find("%rdi"),           string::npos);
    ASSERT_NE(asm_text.find("call printf"),    string::npos);
    ASSERT_EQ(asm_text.find("_start"),         string::npos);
}

TEST(codegen, block_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/017_block.sa.json";
    string asmf = "out/017_block.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // block generates valid asm: printf call and return
    ASSERT_NE(asm_text.find("call printf"), string::npos);
    ASSERT_NE(asm_text.find("\tleave\n\tret\n"), string::npos);
    // no extra block-related assembly instructions leaked
    ASSERT_EQ(asm_text.find("BlockEnter"),  string::npos);
    ASSERT_EQ(asm_text.find("BlockLeave"),  string::npos);
}

TEST(codegen, if_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/019_if_stmt.sa.json";
    string asmf = "out/019_if_stmt.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // condition: testb + je to end label
    ASSERT_NE(asm_text.find("testb"),       string::npos);
    ASSERT_NE(asm_text.find("je "),         string::npos);
    ASSERT_NE(asm_text.find(".Lif0_end:"),  string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
    // no else: no jmp instruction
    ASSERT_EQ(asm_text.find("\tjmp "),      string::npos);
}

TEST(codegen, if_else_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/020_if_else_stmt.sa.json";
    string asmf = "out/020_if_else_stmt.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // conditional jump to else label
    ASSERT_NE(asm_text.find("testb"),            string::npos);
    ASSERT_NE(asm_text.find("je "),              string::npos);
    ASSERT_NE(asm_text.find(".Lif0_else:"),      string::npos);
    ASSERT_NE(asm_text.find(".Lif0_end:"),       string::npos);
    // unconditional jmp to end after then-block
    ASSERT_NE(asm_text.find("\tjmp .Lif0_end"),  string::npos);
    ASSERT_NE(asm_text.find("call printf"),      string::npos);
}

TEST(codegen, unary_minus) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/021_unary_minus.sa.json";
    string asmf = "out/021_unary_minus.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("negq"),        string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, sub_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/022_sub_int32.sa.json";
    string asmf = "out/022_sub_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("subl"),        string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, mul_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/023_mul_int32.sa.json";
    string asmf = "out/023_mul_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("imull"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, neg_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/024_neg_int32.sa.json";
    string asmf = "out/024_neg_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("negl"),        string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, cmp_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/025_cmp_int32.sa.json";
    string asmf = "out/025_cmp_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("cmpl"),        string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, div_rhs_in_rax) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/026_div_rhs_in_rax.sa.json";
    string asmf = "out/026_div_rhs_in_rax.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // The return value is also the divisor; idivq overwrites %rax, so the
    // value must live elsewhere until the return.
    ASSERT_EQ(asm_text.find("movq %rax, %r10"), string::npos);
    ASSERT_NE(asm_text.find("idivq %rbx"),      string::npos);
    ASSERT_NE(asm_text.find("movq %rbx, %rax"), string::npos);
}

TEST(codegen, while_loop) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/027_while_loop.sa.json";
    string asmf = "out/027_while_loop.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // while loop: start/end labels
    ASSERT_NE(asm_text.find(".Lwhile0_start:"),      string::npos);
    ASSERT_NE(asm_text.find(".Lwhile0_end:"),        string::npos);
    // condition test and conditional jump to end
    ASSERT_NE(asm_text.find("testb"),                string::npos);
    ASSERT_NE(asm_text.find("je "),                  string::npos);
    // unconditional back-jump to start
    ASSERT_NE(asm_text.find("\tjmp .Lwhile0_start"), string::npos);
    ASSERT_NE(asm_text.find("call printf"),          string::npos);
}

TEST(codegen, int8_var) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/029_int8_var.sa.json";
    string asmf = "out/029_int8_var.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Int8 variable → 1-byte stack slot: movb instruction and -1(%rbp) offset
    ASSERT_NE(asm_text.find("movb $5, -1(%rbp)"), string::npos);
}

TEST(codegen, void_func) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/028_void_func.sa.json";
    string asmf = "out/028_void_func.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // void function: label present, leave+ret emitted
    ASSERT_NE(asm_text.find("greet:"),   string::npos);
    ASSERT_NE(asm_text.find("leave"),    string::npos);
    ASSERT_NE(asm_text.find("\tret"),    string::npos);
    // caller: call instruction present, but no movq %rax after it
    ASSERT_NE(asm_text.find("call greet"), string::npos);
    // no return-value move: greet returns void so %rax is not copied anywhere
    size_t call_pos = asm_text.find("call greet");
    ASSERT_NE(call_pos, string::npos);
    string after_call = asm_text.substr(call_pos + string("call greet").size(), 30);
    ASSERT_EQ(after_call.find("movq %rax"), string::npos);
}

TEST(codegen, float_var_decl) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/030_float_var_decl.sa.json";
    string asmf = "out/030_float_var_decl.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // float constants in .rodata
    ASSERT_NE(asm_text.find(".double 3.14"), string::npos);
    ASSERT_NE(asm_text.find(".float 1.5"),   string::npos);
    // flo64 load/store
    ASSERT_NE(asm_text.find("movsd"), string::npos);
    // flo32 load/store
    ASSERT_NE(asm_text.find("movss"), string::npos);
    // int literal adopted float type: emitted as float constant in .rodata
    ASSERT_NE(asm_text.find(".double 5"), string::npos);
    ASSERT_NE(asm_text.find(".float 3"),  string::npos);
}

TEST(codegen, float_arg) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/031_float_arg.sa.json";
    string asmf = "out/031_float_arg.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // float constant in .rodata (variable and literal)
    ASSERT_NE(asm_text.find(".double 3.14"), string::npos);
    ASSERT_NE(asm_text.find(".double 1.5"),  string::npos);
    // float arg loaded into xmm0
    ASSERT_NE(asm_text.find("%xmm0"), string::npos);
    // al set to 1 (one float XMM arg)
    ASSERT_NE(asm_text.find("movl $1, %eax"), string::npos);
}

TEST(codegen, float_mixed_args) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/032_float_mixed_args.sa.json";
    string asmf = "out/032_float_mixed_args.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int args go to integer registers, float arg goes to xmm0
    ASSERT_NE(asm_text.find("%rsi"), string::npos);   // first int arg (a)
    ASSERT_NE(asm_text.find("%rdx"), string::npos);   // second int arg (b)
    ASSERT_NE(asm_text.find("%xmm0"), string::npos);  // float arg (x)
    // al = 1 (one float XMM arg)
    ASSERT_NE(asm_text.find("movl $1, %eax"), string::npos);
}

TEST(codegen, uint_convert) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/033_uint_convert.sa.json";
    string asmf = "out/033_uint_convert.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // uint8 var init: 1-byte slot
    ASSERT_NE(asm_text.find("movb $5,"), string::npos);
    // uint8 → uint32 widening: zero-extend byte to long
    ASSERT_NE(asm_text.find("movzbl"), string::npos);
    // uint8 → flo64: zero-extend to 64-bit then convert
    ASSERT_NE(asm_text.find("movzbq"), string::npos);
    // uint16 → flo64: zero-extend word to 64-bit then convert
    ASSERT_NE(asm_text.find("movzwq"), string::npos);
    // uint8/uint16/uint32 → flo64: cvtsi2sdq %rax used in all three paths
    ASSERT_NE(asm_text.find("cvtsi2sdq %rax,"), string::npos);
}

TEST(codegen, int_convert_extra) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/034_int_convert_extra.sa.json";
    string asmf = "out/034_int_convert_extra.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int8 → int16: sign-extend byte to word
    ASSERT_NE(asm_text.find("movsbw"), string::npos);
    // int8 → int64: sign-extend byte to quad
    ASSERT_NE(asm_text.find("movsbq"), string::npos);
    // int16 → int64: sign-extend word to quad
    ASSERT_NE(asm_text.find("movswq"), string::npos);
    // int32 → int8: narrowing (low byte only)
    ASSERT_NE(asm_text.find("movb"), string::npos);
}

TEST(codegen, float32_convert) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/035_float32_convert.sa.json";
    string asmf = "out/035_float32_convert.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // flo64 → flo32
    ASSERT_NE(asm_text.find("cvtsd2ss"), string::npos);
    // flo32 → int32
    ASSERT_NE(asm_text.find("cvttss2sil"), string::npos);
    // flo32 → int64
    ASSERT_NE(asm_text.find("cvttss2siq"), string::npos);
    // flo64 → int32
    ASSERT_NE(asm_text.find("cvttsd2sil"), string::npos);
    // int32 → flo32
    ASSERT_NE(asm_text.find("cvtsi2ssl"), string::npos);
    // int64 → flo32
    ASSERT_NE(asm_text.find("cvtsi2ssq"), string::npos);
    // int8/int16 → float: sign-extend via %rax then convert
    ASSERT_NE(asm_text.find("movsbq"), string::npos);   // int8 path
    ASSERT_NE(asm_text.find("movswq"), string::npos);   // int16 path
}

TEST(codegen, uint_widen_narrow) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/036_uint_widen_narrow.sa.json";
    string asmf = "out/036_uint_widen_narrow.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // uint8 → uint16: zero-extend byte to word
    ASSERT_NE(asm_text.find("movzbw"), string::npos);
    // uint8 → uint64: zero-extend byte to quad
    ASSERT_NE(asm_text.find("movzbq"), string::npos);
    // uint16 → uint32: zero-extend word to long
    ASSERT_NE(asm_text.find("movzwl"), string::npos);
    // uint16 → uint64: zero-extend word to quad
    ASSERT_NE(asm_text.find("movzwq"), string::npos);
    // uint32 → uint64: movl zero-extends implicitly
    // (uint8/16/32 → flo32): cvtsi2ssq %rax
    ASSERT_NE(asm_text.find("cvtsi2ssq %rax,"), string::npos);
}

TEST(codegen, float_arith) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/037_float_arith.sa.json";
    string asmf = "out/037_float_arith.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("addsd"), string::npos);
    ASSERT_NE(asm_text.find("subsd"), string::npos);
    ASSERT_NE(asm_text.find("mulsd"), string::npos);
    ASSERT_NE(asm_text.find("divsd"), string::npos);
    ASSERT_NE(asm_text.find("addss"), string::npos);
    ASSERT_NE(asm_text.find("subss"), string::npos);
    ASSERT_NE(asm_text.find("mulss"), string::npos);
    ASSERT_NE(asm_text.find("divss"), string::npos);
}

TEST(codegen, float_cmp) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/038_float_cmp.sa.json";
    string asmf = "out/038_float_cmp.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("ucomisd"), string::npos);
    ASSERT_NE(asm_text.find("setb"),    string::npos);  // <
    ASSERT_NE(asm_text.find("sete"),    string::npos);  // ==
}

TEST(codegen, float_neg) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/039_float_neg.sa.json";
    string asmf = "out/039_float_neg.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("xorpd"), string::npos);  // flo64 neg
    ASSERT_NE(asm_text.find("xorps"), string::npos);  // flo32 neg
    ASSERT_NE(asm_text.find("\t.p2align 4\n.neg_mask_f64:\n\t.quad 0x8000000000000000, 0\n"), string::npos);
    ASSERT_NE(asm_text.find("\t.p2align 4\n.neg_mask_f32:\n\t.long 0x80000000, 0, 0, 0\n"), string::npos);
}

TEST(codegen, array_buf) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/040_array_buf.sa.json";
    string asmf = "out/040_array_buf.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call malloc"), string::npos);
    ASSERT_NE(asm_text.find("call free"),   string::npos);
    ASSERT_NE(asm_text.find("movq %rax,"),  string::npos);
    ASSERT_NE(asm_text.find("%rdi"),        string::npos);
}

TEST(codegen, arr_rw) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/041_arr_rw.sa.json";
    string asmf = "out/041_arr_rw.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // arr-index read via DerefLoad
    ASSERT_NE(asm_text.find("movl (%r"), string::npos);
    // arr-assign write via DerefStore
    ASSERT_NE(asm_text.find(", (%r"),    string::npos);
}

TEST(codegen, deref_load_reg_addr) {
    // Exercises emitInstrDerefLoad when addr is in a physical register (not stack).
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/042_deref_load_reg.sa.json";
    string asmf = "out/042_deref_load_reg.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movl (%r"), string::npos);
}

TEST(codegen, deref_store_reg_src) {
    // Exercises emitInstrDerefStore when src is in a physical register (not stack).
    // Uses scale=1 (uint8) to keep vreg count low so src stays in a callee-saved register.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/042_arr_rw_scale1.sa.json";
    string asmf = "out/042_arr_rw_scale1.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // src comes from the register itself, not the scratch %al
    ASSERT_NE(asm_text.find("movb %r"), string::npos);
}

TEST(codegen, deref_idx_int32_extend) {
    // Verifies that int32 index is sign-extended (movslq) rather than zero-loaded (movq).
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/043_deref_idx_int32.sa.json";
    string asmf = "out/043_deref_idx_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movslq"), string::npos);
}

TEST(codegen, logical_ops) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/044_logical_ops.sa.json";
    string asmf = "out/044_logical_ops.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // logical-and: short-circuit label and conditional jump
    ASSERT_NE(asm_text.find(".Lland0_end:"),    string::npos);
    ASSERT_NE(asm_text.find("je .Lland0_end"),  string::npos);
    // logical-or: short-circuit labels and jumps
    ASSERT_NE(asm_text.find(".Llor1_true:"),    string::npos);
    ASSERT_NE(asm_text.find(".Llor1_end:"),     string::npos);
    ASSERT_NE(asm_text.find("jne .Llor1_true"), string::npos);
    // logical-not: short-circuit label and conditional jump
    ASSERT_NE(asm_text.find(".Lnot2_end:"),     string::npos);
    ASSERT_NE(asm_text.find("je .Lnot2_end"),   string::npos);
}

TEST(codegen, if_not) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/045_if_not.sa.json";
    string asmf = "out/045_if_not.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);

    // No intermediate .Lnot* label: LogicalNot must not be materialized
    ASSERT_EQ(asm_text.find(".Lnot"),            string::npos);

    // if (!x): condition inverted → jne (not je) to if end label
    ASSERT_NE(asm_text.find("jne .Lif0_end"),    string::npos);
    ASSERT_EQ(asm_text.find("je .Lif0_end"),     string::npos);

    // while (!x): condition inverted → jne to while end label
    ASSERT_NE(asm_text.find("jne .Lwhile1_end"), string::npos);
    ASSERT_NE(asm_text.find(".Lwhile1_start:"),  string::npos);
}

TEST(codegen, if_and) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/046_if_and.sa.json";
    string asmf = "out/046_if_and.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);

    // No .Lland* label: LogicalAnd must not be materialized in branch context
    ASSERT_EQ(asm_text.find(".Lland"), string::npos);

    // if (a && b): both operands jump directly to if end label
    ASSERT_NE(asm_text.find("je .Lif0_end"),      string::npos);

    // while (a && b): both operands jump directly to while end label
    ASSERT_NE(asm_text.find("je .Lwhile1_end"),   string::npos);
    ASSERT_NE(asm_text.find(".Lwhile1_start:"),   string::npos);
}

TEST(codegen, if_or) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/047_if_or.sa.json";
    string asmf = "out/047_if_or.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);

    // No _true: label: LogicalOr must not be materialized in branch context
    ASSERT_EQ(asm_text.find("_true:"),          string::npos);

    // if (a || b): left true → jne to skip label; right false → je to if end
    ASSERT_NE(asm_text.find("jne .Llor"),       string::npos);
    ASSERT_NE(asm_text.find("je .Lif0_end"),    string::npos);

    // while (a || b): right false → je to while end
    ASSERT_NE(asm_text.find("je .Lwhile2_end"), string::npos);
    ASSERT_NE(asm_text.find(".Lwhile2_start:"), string::npos);
}

TEST(codegen, if_or_complex) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/048_if_or_complex.sa.json";
    string asmf = "out/048_if_or_complex.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);

    // if (!a || b): lowerBranchCondTrue(LogicalNot) → je to lor skip label
    ASSERT_NE(asm_text.find("je .Llor1_skip"),  string::npos);
    ASSERT_NE(asm_text.find("je .Lif0_end"),    string::npos);

    // if ((a || b) || c): lowerBranchCondTrue(LogicalOr) → two jne to lor skip label
    ASSERT_NE(asm_text.find("jne .Llor3_skip"), string::npos);
    ASSERT_NE(asm_text.find("je .Lif2_end"),    string::npos);

    // if ((a && b) || c): lowerBranchCondTrue(LogicalAnd) → .Land inner skip label
    ASSERT_NE(asm_text.find(".Land6_skip:"),    string::npos);
    ASSERT_NE(asm_text.find("jne .Llor5_skip"), string::npos);
    ASSERT_NE(asm_text.find("je .Lif4_end"),    string::npos);
}

TEST(codegen, deref_idx_small_types) {
    // Both register- and stack-slot-held index operands, for every small
    // integer type, sign/zero-extend correctly before use as an array index.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/049_deref_idx_small_types.sa.json";
    string asmf = "out/049_deref_idx_small_types.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);

    ASSERT_NE(asm_text.find("movsbq %sil, %r11"),     string::npos);
    ASSERT_NE(asm_text.find("movsbq -1(%rbp), %r11"), string::npos);
    ASSERT_NE(asm_text.find("movswq %si, %r11"),      string::npos);
    ASSERT_NE(asm_text.find("movswq -2(%rbp), %r11"), string::npos);
    ASSERT_NE(asm_text.find("movzbl %sil, %r11d"),    string::npos);
    ASSERT_NE(asm_text.find("movzbl -8(%rbp), %r11d"), string::npos);
    ASSERT_NE(asm_text.find("movzwl %si, %r11d"),     string::npos);
    ASSERT_NE(asm_text.find("movzwl -8(%rbp), %r11d"), string::npos);
    // Uint32 needs no dedicated zero-extend mnemonic: x86-64 movl already
    // clears the upper 32 bits of the destination register.
    ASSERT_NE(asm_text.find("movl -8(%rbp), %r11d"),  string::npos);
}

TEST(codegen, embed_arr) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/050_embed_arr.sa.json";
    string asmf = "out/050_embed_arr.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Row stride multiply: inner-size(4) * sizeof(int32)(4) = 16
    ASSERT_NE(asm_text.find("imulq $16"), string::npos);
    ASSERT_NE(asm_text.find("movl (%r"),  string::npos);  // element read: DerefLoad
    ASSERT_NE(asm_text.find(", (%r"),     string::npos);  // element write: DerefStore
}

TEST(codegen, embed_arr_var_row) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/051_embed_arr_var_row.sa.json";
    string asmf = "out/051_embed_arr_var_row.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Variable-stride row access is a runtime multiply (register x register), not an immediate.
    ASSERT_NE(asm_text.find("imulq %r"),   string::npos);
    ASSERT_NE(asm_text.find("addq "),      string::npos);
    ASSERT_NE(asm_text.find("movl (%r"),   string::npos);
}

TEST(codegen, float_in_block) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/052_float_in_block.sa.json";
    string asmf = "out/052_float_in_block.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Both a FloLit and an IntLit-as-float (flo64 y = 2) declared inside a
    // block must be tracked by blockVarStack_ for scope cleanup.
    ASSERT_NE(asm_text.find(".double 3.14"), string::npos);
    ASSERT_NE(asm_text.find(".double 2"),    string::npos);
    ASSERT_NE(asm_text.find("movsd"),        string::npos);
}

TEST(codegen, field_access) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/053_field_access.sa.json";
    string asmf = "out/053_field_access.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find(", (%r"),  string::npos);  // DerefStore, offset 0
    ASSERT_NE(asm_text.find("8(%r"),   string::npos);  // offset-8 field, load and store
    ASSERT_NE(asm_text.find("(%r"),    string::npos);  // DerefLoad, offset 0
}

// r.tl.x / r.tl.y: r holds a pointer to Point at offset 0, so each access
// first DerefLoads r[0] to get the pointer, then DerefStores/DerefLoads
// through it at offsets 0 and 8.
TEST(codegen, ptr_field_access) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/058_ptr_field_access.sa.json";
    string asmf = "out/058_ptr_field_access.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("(%r"),  string::npos);
    ASSERT_NE(asm_text.find("8(%r"), string::npos);  // field y, offset 8
}

// Verify that Mixed { int32 a; int64 b; } is laid out with C ABI offsets:
//   a at offset 0  (int32 → movl, no numeric prefix)
//   b at offset 8  (int64 → movq, 4-byte padding proven by offset 8 not 4)
//   total size 16  (calloc second arg = $16)
TEST(codegen, struct_c_abi_layout) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/055_struct_c_abi_layout.sa.json";
    string asmf = "out/055_struct_c_abi_layout.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("$16,"),   string::npos);
    ASSERT_NE(asm_text.find("movl"),   string::npos);
    ASSERT_NE(asm_text.find(", (%r"),  string::npos);
    ASSERT_NE(asm_text.find("(%r"),    string::npos);
    ASSERT_NE(asm_text.find("8(%r"),   string::npos);
}

// Verify that Trailing { int64 a; int32 b; } has trailing padding:
//   a at offset 0  (int64 → movq)
//   b at offset 8  (int32 → movl)
//   total size 16  (not 12 — 4 trailing bytes pad to max-align multiple)
TEST(codegen, struct_trailing_pad) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/056_struct_trailing_pad.sa.json";
    string asmf = "out/056_struct_trailing_pad.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("$16,"),   string::npos);
    ASSERT_EQ(asm_text.find("$12,"),   string::npos);
    ASSERT_NE(asm_text.find("movq"),   string::npos);
    ASSERT_NE(asm_text.find(", (%r"),  string::npos);
    ASSERT_NE(asm_text.find("movl"),   string::npos);
    ASSERT_NE(asm_text.find(" 8(%r"),  string::npos);
    ASSERT_EQ(asm_text.find(" 4(%r"),  string::npos);
}

// Verify that Multi { int8 a; int32 b; int64 c; } has two padding gaps:
//   a at offset 0  (int8  → movb)
//   b at offset 4  (int32 → movl)  — 3-byte pad after a
//   c at offset 8  (int64 → movq)
//   total size 16
TEST(codegen, struct_multi_pad) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/057_struct_multi_pad.sa.json";
    string asmf = "out/057_struct_multi_pad.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("$16,"),  string::npos);
    ASSERT_NE(asm_text.find("movb"),  string::npos);
    ASSERT_NE(asm_text.find("(%r"),   string::npos);
    ASSERT_NE(asm_text.find("movl"),  string::npos);
    ASSERT_NE(asm_text.find("4(%r"),  string::npos);
    ASSERT_NE(asm_text.find("movq"),  string::npos);
    ASSERT_NE(asm_text.find("8(%r"),  string::npos);
}

// Six struct pointers exhaust the callee-saved registers, forcing p6 to spill to the stack.
TEST(codegen, deref_stack_spill) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/054_deref_stack_spill.sa.json";
    string asmf = "out/054_deref_stack_spill.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("(%rbp), %r10"), string::npos);  // p6 reloaded before deref
    ASSERT_NE(asm_text.find(", (%r10)"),     string::npos);  // DerefStore via reload
    ASSERT_NE(asm_text.find("(%r10),"),      string::npos);  // DerefLoad via reload
}

// Six spilled Widget struct pointers force w.tris[0].x through the
// address-only CalcAddr path (not DerefLoad/DerefStore) while spilled.
TEST(codegen, calcaddr_stack_spill) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/059_calcaddr_stack_spill.sa.json";
    string asmf = "out/059_calcaddr_stack_spill.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Spilled ptr reload immediately followed by an address-only leaq (no
    // dereference), matching emitInstrCalcAddr's stack branch.
    ASSERT_NE(asm_text.find("(%rbp), %r10\n\tleaq (%r10),"), string::npos);
}

// Regression: an address-taken CallC result (`x`, then `@x`) must be forced
// to a stack slot rather than left in a register.
TEST(codegen, addr_of_forces_stack) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/060_addr_of_forces_stack.sa.json";
    string asmf = "out/060_addr_of_forces_stack.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // x's CallC result goes straight to a stack slot (not %rax or a
    // callee-saved register), and its address loads from that same slot
    // into use_ptr's first argument register.
    ASSERT_NE(asm_text.find("call get_val\n\tmovq %rax, -8(%rbp)\n"), string::npos);
    ASSERT_NE(asm_text.find("leaq -8(%rbp), %rdi\n"), string::npos);
}

// An address-taken variable with no initializer (`int64 x;`) still gets a stack slot.
TEST(codegen, addr_of_no_init) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/061_addr_of_no_init.sa.json";
    string asmf = "out/061_addr_of_no_init.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // x gets a stack slot even though it's never written to before its address
    // is taken and passed to set_val.
    ASSERT_NE(asm_text.find("leaq -8(%rbp), %rdi\n"), string::npos);
}

// Twelve simultaneous addr-of results exhaust the registers, forcing the
// twelfth LeaLocal destination itself to spill to the stack.
TEST(codegen, addr_of_dst_stack_spill) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/062_addr_of_dst_stack_spill.sa.json";
    string asmf = "out/062_addr_of_dst_stack_spill.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("(%rbp), %r11\n\tmovq %r11, "), string::npos);
}

TEST(codegen, sign_cross_widen) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/063_sign_cross_widen.sa.json";
    string asmf = "out/063_sign_cross_widen.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Widening must extend according to the SOURCE's signedness: unsigned
    // sources zero-extend even when the destination type is signed.
    ASSERT_NE(asm_text.find("movzbq"), string::npos);  // uint8 -> int64
    ASSERT_NE(asm_text.find("movzwq"), string::npos);  // uint16 -> int64
    ASSERT_EQ(asm_text.find("movsbq"), string::npos);  // must not sign-extend
    ASSERT_EQ(asm_text.find("movswq"), string::npos);
    ASSERT_EQ(asm_text.find("movslq"), string::npos);  // uint32 -> int64 must not sign-extend
}

TEST(codegen, sign_cross_narrow) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/064_sign_cross_narrow.sa.json";
    string asmf = "out/064_sign_cross_narrow.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // Widening must extend according to the SOURCE's signedness: signed
    // sources sign-extend even when the destination type is unsigned.
    ASSERT_NE(asm_text.find("movsbq"), string::npos);  // int8 -> uint64
    ASSERT_NE(asm_text.find("movswl"), string::npos);  // int16 -> uint32
    ASSERT_EQ(asm_text.find("movzbq"), string::npos);  // must not zero-extend
    ASSERT_EQ(asm_text.find("movzwl"), string::npos);
    // Narrowing (int64 -> uint8) and same-width sign reinterpretation
    // (int32 -> uint32) just reference the low bits; no dedicated mnemonic.
    ASSERT_NE(asm_text.find("movb"), string::npos);
    ASSERT_NE(asm_text.find("movl"), string::npos);
}

TEST(codegen, float_to_uint) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/065_float_to_uint.sa.json";
    string asmf = "out/065_float_to_uint.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // flo64 -> uint32/uint8: must use the 64-bit form (32-bit form's signed
    // result cannot represent a uint32 above INT32_MAX).
    ASSERT_NE(asm_text.find("cvttsd2siq"), string::npos);
    // flo64 -> int8: signed narrow destinations reuse the 32-bit form.
    ASSERT_NE(asm_text.find("cvttsd2sil"), string::npos);
}

TEST(codegen, uint32_arith) {
    // Regression: Uint32 arithmetic used to emit 64-bit-width instructions
    // (addq/imulq) against 32-bit-sized registers, which the assembler rejects.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/066_uint32_arith.sa.json";
    string asmf = "out/066_uint32_arith.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("addl"),  string::npos);
    ASSERT_NE(asm_text.find("imull"), string::npos);
    ASSERT_EQ(asm_text.find("addq %r"),  string::npos);
    ASSERT_EQ(asm_text.find("imulq %r"), string::npos);
}

TEST(codegen, uint_lit_narrow) {
    // Regression: a uint32 initialized from a `u`-suffixed literal used to be
    // treated as Uint64 regardless of its declared 32-bit width.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/067_uint_lit_narrow.sa.json";
    string asmf = "out/067_uint_lit_narrow.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movl $4042322160"), string::npos);
    ASSERT_NE(asm_text.find("addl"), string::npos);
    ASSERT_EQ(asm_text.find("movq $4042322160"), string::npos);
    ASSERT_EQ(asm_text.find("addq %r"), string::npos);
}

TEST(codegen, bitwise_ops) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/068_bitwise_ops.sa.json";
    string asmf = "out/068_bitwise_ops.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("andq"), string::npos);
    ASSERT_NE(asm_text.find("orq"),  string::npos);
    ASSERT_NE(asm_text.find("xorq"), string::npos);
    ASSERT_NE(asm_text.find("notq"), string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, c_global) {
    // A name bound to a static slot (here the C global `stderr`) is read with
    // LeaLabel+DerefLoad, with no dedicated VInstr of its own.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/069_c_global.sa.json";
    string asmf = "out/069_c_global.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("leaq stderr(%rip), %r"), string::npos);
    ASSERT_NE(asm_text.find("movq (%r"), string::npos);
    ASSERT_NE(asm_text.find("call fprintf"), string::npos);
}

// Fixture-only functions (get_flo/get_mixed/get_big, unresolved at link
// time) cover every SysV struct-return eightbyte class, so these only
// assemble the output, never link it.

TEST(codegen, struct_ret_sse) {
    // 1 eightbyte, all SSE: struct { flo64 x; } returned in %xmm0 only.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/070_struct_ret_sse.sa.json";
    string asmf = "out/070_struct_ret_sse.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call get_flo"),  string::npos);
    ASSERT_NE(asm_text.find("movsd %xmm0,"),  string::npos);
    ASSERT_EQ(asm_text.find("%rdx"),          string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/070_struct_ret_sse.o"), "");
}

TEST(codegen, struct_ret_mixed) {
    // 2 eightbytes, INTEGER+SSE mixed: struct { int64 a; flo64 b; } returned
    // in %rax/%xmm0, second eightbyte stored at offset 8.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/071_struct_ret_mixed.sa.json";
    string asmf = "out/071_struct_ret_mixed.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call get_mixed"), string::npos);
    ASSERT_NE(asm_text.find("movq %rax,"),     string::npos);
    ASSERT_NE(asm_text.find("movsd %xmm0,"),   string::npos);
    ASSERT_NE(asm_text.find(", 8(%r"),         string::npos);
    ASSERT_EQ(asm_text.find("%xmm1,"),         string::npos);
    ASSERT_EQ(asm_text.find("%xmm1\n"),        string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/071_struct_ret_mixed.o"), "");
}

TEST(codegen, struct_ret_flo64) {
    // {flo64 a; flo32 b; flo32 c;} returned from a ret-slot: the eightbyte
    // marked flo64 is read whole, the two flo32 halves are read and packed.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/079_struct_ret_flo64.sa.json";
    string asmf = "out/079_struct_ret_flo64.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("\tmovsd ("), string::npos);
    ASSERT_EQ(asm_text.find("\tmovss ("), string::npos);
    ASSERT_EQ(asm_text.find("\tmovss 4("), string::npos);
    size_t pack = asm_text.find("unpcklps");
    ASSERT_NE(pack, string::npos);
    ASSERT_EQ(asm_text.find("unpcklps", pack + 1), string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/079_struct_ret_flo64.o"), "");
}

TEST(codegen, struct_ret_memory) {
    // >16 bytes: MEMORY class -- the ret-dest pointer is passed as the hidden
    // first argument; no register-based eightbyte store follows the call.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/072_struct_ret_memory.sa.json";
    string asmf = "out/072_struct_ret_memory.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq %rax, %rdi"), string::npos);
    ASSERT_NE(asm_text.find("call get_big"),    string::npos);
    ASSERT_EQ(asm_text.find("%xmm"),            string::npos);
    ASSERT_NE(asm_text.find("call get_big\n\tleave\n"), string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/072_struct_ret_memory.o"), "");
}

TEST(codegen, struct_ret_int_tail_widths) {
    // Exercises movb/movw/movl store widths for a fractional-size (1/2/4-byte)
    // INTEGER eightbyte, which no glibc function actually returns.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/073_struct_ret_int_tail_widths.sa.json";
    string asmf = "out/073_struct_ret_int_tail_widths.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call get_tinya"), string::npos);
    ASSERT_NE(asm_text.find("movb %al,"),      string::npos);
    ASSERT_NE(asm_text.find("call get_tinyb"), string::npos);
    ASSERT_NE(asm_text.find("movw %ax,"),      string::npos);
    ASSERT_NE(asm_text.find("call get_tinyc"), string::npos);
    ASSERT_NE(asm_text.find("movl %eax,"),     string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/073_struct_ret_int_tail_widths.o"), "");
}

TEST(codegen, func_ref) {
    // A Palan function passed as a C callback (func-ref, e.g. qsort's
    // comparator) lowers to a bare LeaLabel of its own name, same as a string literal.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/074_func_ref.sa.json";
    string asmf = "out/074_func_ref.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("leaq cmp(%rip)"), string::npos);
    ASSERT_NE(asm_text.find("call qsort"), string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/074_func_ref.o"), "");
}

TEST(codegen, syscall) {
    // Raw Linux syscall ABI: number in %rax, the syscall argument table
    // (%r10 in 4th position, never %rcx), and the `syscall` instruction.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/075_syscall.sa.json";
    string asmf = "out/075_syscall.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // getpid at the top level: result must survive the later pwrite64/exit_group
    // calls, so it lands in a callee-saved register -- a real copy out of %eax.
    ASSERT_NE(asm_text.find("movl $39, %eax\n\tsyscall\n\tmovl %eax, %"), string::npos);
    // getpid inside get_pid_wrap, whose sole statement returns it directly:
    // the result stays in %eax across both the CallSys result copy and
    // RetPln, eliding both (no "movl %eax, %eax").
    ASSERT_NE(asm_text.find("movl $39, %eax\n\tsyscall\n\tleave\n\tret\n"), string::npos);
    // pwrite64: 4 arguments, the 4th in %r10; its result is never read, so
    // no copy is emitted (the dead-dst guard elides it entirely).
    ASSERT_NE(asm_text.find("movl $18, %eax\n\tsyscall\n"), string::npos);
    ASSERT_NE(asm_text.find(", %rsi\n"),  string::npos);
    ASSERT_NE(asm_text.find(", %r10\n"),  string::npos);
    ASSERT_EQ(asm_text.find("%rcx"),      string::npos);
    // exit_group: argument only, no result to copy back.
    ASSERT_NE(asm_text.find("movl $231, %eax\n\tsyscall\n"), string::npos);
    ASSERT_EQ(countOccurrences(asm_text, "\tsyscall\n"),    4u);
    ASSERT_EQ(countOccurrences(asm_text, "\tcall "),        1u);  // get_pid_wrap

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/075_syscall.o"), "");
}

TEST(codegen, entry) {
    // Palan links no crt startup objects, so the entry object itself must
    // supply __dso_handle and .note.GNU-stack, the ELF/libc glue those objects
    // would otherwise provide.
    cleanTestEnv();
    string asmf = "out/entry.s";

    string err = execTestCommand("palan-codegen --entry app..init --entry app..main -o " + asmf);
    ASSERT_EQ(err, "");

    // Each call reloads argc and argv, which the one before may clobber.
    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("\t.globl _start\n_start:\n"
                            "\tmovq (%rsp), %rdi\n\tleaq 8(%rsp), %rsi\n\tcall app..init\n"
                            "\tmovq (%rsp), %rdi\n\tleaq 8(%rsp), %rsi\n\tcall app..main\n"
                            "\tmovl $0, %edi\n\tcall exit\n"), string::npos);
    ASSERT_NE(asm_text.find("\t.section .data\n"),         string::npos);
    ASSERT_NE(asm_text.find("\t.globl __dso_handle\n"),    string::npos);
    ASSERT_NE(asm_text.find("\t.hidden __dso_handle\n"),   string::npos);
    ASSERT_NE(asm_text.find("__dso_handle:\n\t.quad 0\n"), string::npos);
    ASSERT_NE(asm_text.find(".note.GNU-stack"),            string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/entry.o"), "");
}

TEST(codegen, entry_usage) {
    cleanTestEnv();
    string usage = "Usage: palan-codegen -e <symbol> -o <file>";
    ASSERT_NE(execTestCommand("palan-codegen --entry app..main").find(usage), string::npos);
    ASSERT_NE(execTestCommand("palan-codegen --entry app..main -o out/entry.s "
                              "../test/testdata/codegen/001_helloworld.sa.json").find(usage), string::npos);
    ASSERT_NE(execTestCommand("palan-codegen --entry app..main -o out/no-such-dir/entry.s")
              .find("Could not open output file"), string::npos);
}

TEST(codegen, module_object_crt_glue) {
    // A module object is the same whichever module is the entry: it must not
    // define _start or __dso_handle (exactly one per link), but
    // .note.GNU-stack stays unconditional.
    cleanTestEnv();
    string asmf = "out/002_module_crt_glue.s";

    string err = run_codegen("../test/testdata/codegen/002_printf_int_literal.sa.json", asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_EQ(asm_text.find("__dso_handle"),    string::npos);
    ASSERT_EQ(asm_text.find("_start"),          string::npos);
    ASSERT_NE(asm_text.find(".note.GNU-stack"), string::npos);
}

TEST(codegen, uint64_float_convert) {
    // SSE converts cover only the signed int64 range, so each direction is
    // lowered into a branch on the top bit / the 2^63 threshold.
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/076_uint64_float_convert.sa.json";
    string asmf = "out/076_uint64_float_convert.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find(".Lu2f0_hi:"), string::npos);
    ASSERT_NE(asm_text.find(".Lu2f1_hi:"), string::npos);
    ASSERT_NE(asm_text.find(".Lf2u2_hi:"), string::npos);
    ASSERT_NE(asm_text.find(".Lf2u3_hi:"), string::npos);
    ASSERT_NE(asm_text.find("cvtsi2sdq"),  string::npos);
    ASSERT_NE(asm_text.find("cvtsi2ssq"),  string::npos);
    ASSERT_NE(asm_text.find("divq"),       string::npos);
    ASSERT_NE(asm_text.find("cvttsd2siq"), string::npos);
    ASSERT_NE(asm_text.find("cvttss2siq"), string::npos);
    ASSERT_NE(asm_text.find("movabsq $-9223372036854775808"), string::npos);
    ASSERT_NE(asm_text.find("xorq"),       string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/076_uint64_float_convert.o"), "");
}

TEST(codegen, frame_alloc)
{
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/077_frame_alloc.sa.json";
    string asmf = "out/077_frame_alloc.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    // Each `$` variable is the address of its own block in the frame.
    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("leaq -88(%rbp)"), string::npos);
    ASSERT_NE(asm_text.find("leaq -96(%rbp)"), string::npos);
    ASSERT_NE(asm_text.find("leaq -32(%rbp)"), string::npos);  // below the top level's two callee-saved slots
    ASSERT_NE(asm_text.find("call memset"), string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/077_frame_alloc.o"), "");
}

TEST(codegen, global_var)
{
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/078_global.sa.json";
    string asmf = "out/078_global.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // A slot per global, and the `$P` global's storage, in .bss.
    ASSERT_NE(asm_text.find(".section .bss"), string::npos);
    ASSERT_NE(asm_text.find("_078_global.n:\n\t.zero 8"), string::npos);
    ASSERT_NE(asm_text.find("_078_global.p.data:\n\t.zero 16"), string::npos);
    // Every access goes through the label, a C global's as well.
    ASSERT_NE(asm_text.find("leaq _078_global.n(%rip)"), string::npos);
    ASSERT_NE(asm_text.find("leaq _078_global.p(%rip)"), string::npos);
    ASSERT_NE(asm_text.find("leaq optind(%rip)"), string::npos);

    ASSERT_EQ(execTestCommand("as " + asmf + " -o out/078_global.o"), "");
}
