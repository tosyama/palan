/// x86-64 memory access emission: array-indexed deref/store, offset-based
/// deref/store (struct fields), address-of computations.
///
/// @file PlnX86Mem.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"
#include <iostream>
#include <cstdlib>

using namespace std;

// The memory operand of base[idx*scale]. SIB scales stop at 8, so a wider
// stride (a flo80's 16) is computed into %r11 first.
static string indexedAddr(ostream& out, const string& base_reg, const string& idx_reg, int scale)
{
    if (scale <= 8)
        return "(" + base_reg + ", " + idx_reg + ", " + std::to_string(scale) + ")";
    out << "\timulq $" << scale << ", " << idx_reg << ", %r11\n";
    out << "\taddq " << base_reg << ", %r11\n";
    return "(%r11)";
}

// Emits sign/zero-extend instruction to load index into %r11.
// For Int64/Uint64/Ptr64/Uint32 in a physical register, returns the register directly (no instruction).
static string loadIdxR11(ostream& out, VRegType idx_type, const PhysLoc& loc)
{
    if (loc.isStack()) {
        string slot = std::to_string(loc.stackOffset) + "(%rbp)";
        switch (idx_type) {
            case VRegType::Int8:   out << "\tmovsbq " << slot << ", %r11\n"; break;
            case VRegType::Int16:  out << "\tmovswq " << slot << ", %r11\n"; break;
            case VRegType::Int32:  out << "\tmovslq " << slot << ", %r11\n"; break;
            case VRegType::Uint8:  out << "\tmovzbl " << slot << ", %r11d\n"; break;
            case VRegType::Uint16: out << "\tmovzwl " << slot << ", %r11d\n"; break;
            case VRegType::Uint32: out << "\tmovl "   << slot << ", %r11d\n"; break;
            default:               out << "\tmovq "   << slot << ", %r11\n"; break;
        }
        return "%r11";
    }
    const string& reg = loc.base;
    switch (idx_type) {
        case VRegType::Int8:   out << "\tmovsbq " << sizedRegName(reg, VRegType::Int8)   << ", %r11\n";  break;
        case VRegType::Int16:  out << "\tmovswq " << sizedRegName(reg, VRegType::Int16)  << ", %r11\n";  break;
        case VRegType::Int32:  out << "\tmovslq " << sizedRegName(reg, VRegType::Int32)  << ", %r11\n";  break;
        case VRegType::Uint8:  out << "\tmovzbl " << sizedRegName(reg, VRegType::Uint8)  << ", %r11d\n"; break;
        case VRegType::Uint16: out << "\tmovzwl " << sizedRegName(reg, VRegType::Uint16) << ", %r11d\n"; break;
        default:
            return reg;  // Uint32/Int64/Uint64/Ptr64: x86-64 guarantees upper bits valid
    }
    return "%r11";
}

void PlnX86CodeGen::emitInstrDerefLoadIdx(const DerefLoadIdx& dl, const RegMap& rm)
{
    if (!rm.count(dl.dst) || !rm.count(dl.base) || !rm.count(dl.idx)) return;
    const PhysLoc& base_loc = rm.at(dl.base);
    const PhysLoc& idx_loc  = rm.at(dl.idx);
    const PhysLoc& dst_loc  = rm.at(dl.dst);
    const char*    mov      = movInstrForType(dl.type);

    string base_reg;
    if (!base_loc.isStack()) {
        base_reg = base_loc.base;
    } else {
        out << "\tmovq " << base_loc.stackOffset << "(%rbp), %r10\n";
        base_reg = "%r10";
    }

    string idx_reg = loadIdxR11(out, dl.idx_type, idx_loc);

    string addr = indexedAddr(out, base_reg, idx_reg, dl.scale);
    if (!dst_loc.isStack()) {
        out << "\t" << mov << " " << addr << ", "
            << sizedRegName(dst_loc.base, dl.type) << "\n";
    } else {
        string scratch = movedViaXmm(dl.type) ? "%xmm8" : sizedRegName("%rax", dl.type);
        out << "\t" << mov << " " << addr << ", " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", "
            << dst_loc.stackOffset << "(%rbp)\n";
    }
}

void PlnX86CodeGen::emitInstrDerefStoreIdx(const DerefStoreIdx& ds, const RegMap& rm)
{
    if (!rm.count(ds.base) || !rm.count(ds.idx) || !rm.count(ds.src)) return;
    const PhysLoc& base_loc = rm.at(ds.base);
    const PhysLoc& idx_loc  = rm.at(ds.idx);
    const PhysLoc& src_loc  = rm.at(ds.src);
    const char*    mov      = movInstrForType(ds.type);

    string base_reg;
    if (!base_loc.isStack()) {
        base_reg = base_loc.base;
    } else {
        out << "\tmovq " << base_loc.stackOffset << "(%rbp), %r10\n";
        base_reg = "%r10";
    }

    string idx_reg = loadIdxR11(out, ds.idx_type, idx_loc);

    string addr = indexedAddr(out, base_reg, idx_reg, ds.scale);
    if (!src_loc.isStack()) {
        out << "\t" << mov << " " << sizedRegName(src_loc.base, ds.type)
            << ", " << addr << "\n";
    } else {
        string scratch = movedViaXmm(ds.type) ? "%xmm8" : sizedRegName("%rax", ds.type);
        out << "\t" << mov << " " << src_loc.stackOffset << "(%rbp), " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << addr << "\n";
    }
}

void PlnX86CodeGen::emitInstrCalcAddrIdx(const CalcAddrIdx& ca, const RegMap& rm)
{
    if (!rm.count(ca.dst) || !rm.count(ca.base) || !rm.count(ca.idx)) return;
    const PhysLoc& base_loc = rm.at(ca.base);
    const PhysLoc& idx_loc  = rm.at(ca.idx);
    const PhysLoc& dst_loc  = rm.at(ca.dst);

    string base_reg;
    if (!base_loc.isStack()) {
        base_reg = base_loc.base;
    } else {
        out << "\tmovq " << base_loc.stackOffset << "(%rbp), %r10\n";
        base_reg = "%r10";
    }

    // Sign/zero-extend index into idx_reg (%r11 or original register for 64-bit types).
    string idx_reg = loadIdxR11(out, ca.idx_type, idx_loc);

    // SIB byte only allows scale 1/2/4/8; use imulq for arbitrary strides (e.g. 16).
    out << "\timulq $" << ca.scale << ", " << idx_reg << ", %r11\n";
    out << "\taddq " << base_reg << ", %r11\n";

    if (!dst_loc.isStack())
        out << "\tmovq %r11, " << dst_loc.base << "\n";
    else
        out << "\tmovq %r11, " << dst_loc.stackOffset << "(%rbp)\n";
}

void PlnX86CodeGen::emitInstrDerefLoad(const DerefLoad& dl, const RegMap& rm)
{
    if (!rm.count(dl.dst) || !rm.count(dl.ptr)) return;
    const PhysLoc& ptr_loc = rm.at(dl.ptr);
    const PhysLoc& dst_loc = rm.at(dl.dst);
    const char* mov = movInstrForType(dl.type);

    string ptr_reg;
    if (!ptr_loc.isStack()) {
        ptr_reg = ptr_loc.base;
    } else {
        out << "\tmovq " << ptr_loc.stackOffset << "(%rbp), %r10\n";
        ptr_reg = "%r10";
    }

    string addr = (dl.offset ? std::to_string(dl.offset) : "") + "(" + ptr_reg + ")";

    if (!dst_loc.isStack()) {
        out << "\t" << mov << " " << addr << ", "
            << sizedRegName(dst_loc.base, dl.type) << "\n";
    } else {
        string scratch = movedViaXmm(dl.type) ? "%xmm8" : sizedRegName("%rax", dl.type);
        out << "\t" << mov << " " << addr << ", " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", "
            << dst_loc.stackOffset << "(%rbp)\n";
    }
}

void PlnX86CodeGen::emitInstrCalcAddr(const CalcAddr& ca, const RegMap& rm)
{
    if (!rm.count(ca.dst) || !rm.count(ca.ptr)) return;
    const PhysLoc& ptr_loc = rm.at(ca.ptr);
    const PhysLoc& dst_loc = rm.at(ca.dst);

    string ptr_reg;
    if (!ptr_loc.isStack()) {
        ptr_reg = ptr_loc.base;
    } else {
        out << "\tmovq " << ptr_loc.stackOffset << "(%rbp), %r10\n";
        ptr_reg = "%r10";
    }

    string addr = (ca.offset ? std::to_string(ca.offset) : "") + "(" + ptr_reg + ")";

    if (!dst_loc.isStack()) {
        out << "\tleaq " << addr << ", " << dst_loc.base << "\n";
    } else {
        out << "\tleaq " << addr << ", %r11\n";
        out << "\tmovq %r11, " << dst_loc.stackOffset << "(%rbp)\n";
    }
}

void PlnX86CodeGen::emitInstrLeaLocal(const LeaLocal& ll, const RegMap& rm)
{
    if (!rm.count(ll.dst) || !rm.count(ll.local)) return;
    const PhysLoc& local_loc = rm.at(ll.local);
    const PhysLoc& dst_loc   = rm.at(ll.dst);

    if (!local_loc.isStack()) {
        // RegAlloc must force every address-taken local to a stack slot (isVar).
        std::cerr << "PlnX86CodeGen: address-taken local is not on the stack\n";
        std::abort();
    }

    string addr = std::to_string(local_loc.stackOffset) + "(%rbp)";
    if (!dst_loc.isStack()) {
        out << "\tleaq " << addr << ", " << dst_loc.base << "\n";
    } else {
        out << "\tleaq " << addr << ", %r11\n";
        out << "\tmovq %r11, " << dst_loc.stackOffset << "(%rbp)\n";
    }
}

void PlnX86CodeGen::emitInstrFrameAddr(const FrameAddr& fa, const RegMap& rm, const vector<int>& blockOffsets)
{
    if (!rm.count(fa.dst)) return;
    const PhysLoc& dst_loc = rm.at(fa.dst);
    string addr = std::to_string(blockOffsets.at(fa.block)) + "(%rbp)";
    if (!dst_loc.isStack()) {
        out << "\tleaq " << addr << ", " << dst_loc.base << "\n";
    } else {
        out << "\tleaq " << addr << ", %r11\n";
        out << "\tmovq %r11, " << dst_loc.stackOffset << "(%rbp)\n";
    }
}

void PlnX86CodeGen::emitInstrDerefStore(const DerefStore& ds, const RegMap& rm)
{
    if (!rm.count(ds.ptr) || !rm.count(ds.src)) return;
    const PhysLoc& ptr_loc = rm.at(ds.ptr);
    const PhysLoc& src_loc = rm.at(ds.src);
    const char* mov = movInstrForType(ds.type);

    string ptr_reg;
    if (!ptr_loc.isStack()) {
        ptr_reg = ptr_loc.base;
    } else {
        out << "\tmovq " << ptr_loc.stackOffset << "(%rbp), %r10\n";
        ptr_reg = "%r10";
    }

    string addr = (ds.offset ? std::to_string(ds.offset) : "") + "(" + ptr_reg + ")";

    if (!src_loc.isStack()) {
        out << "\t" << mov << " " << sizedRegName(src_loc.base, ds.type)
            << ", " << addr << "\n";
    } else {
        string scratch = movedViaXmm(ds.type) ? "%xmm8" : sizedRegName("%rax", ds.type);
        out << "\t" << mov << " " << src_loc.stackOffset << "(%rbp), " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << addr << "\n";
    }
}
