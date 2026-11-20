/// Register allocation: VProg -> RegMap.
///
/// @file PlnRegAlloc.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnRegAlloc.h"
#include <algorithm>
#include <cassert>

using namespace std;

// Visitor helper: overloaded{f1, f2, ...} dispatches std::visit to the matching overload.
// All VInstr types must be handled; omitting one causes a compile error.
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

RegAllocResult allocateRegisters(const VFunc& func, const PhysRegs& phys)
{
    // Step 1: Collect VReg metadata.
    struct VRegMeta {
        VRegType type         = VRegType::Int64;
        int      def_idx      = -1;
        int      first_def    = -1;
        bool     isVar        = false;   // true if a declared variable (InitVar/InitVarF/DeclVar)
        bool     addrTaken    = false;
        vector<int> writes;              // instructions writing it (defs and Mov into it)
        vector<int> accesses;            // every operand occurrence, writes included
        int      last_any_use = -1;      // last use by non-call-argument instructions
        vector<pair<int,string>> call_uses; // (call_instr_idx, desired arg register)
        bool     isRetValue   = false;   // true if used by RetPln
        int      live_end     = -1;      // last instruction the value must survive to
    };
    map<VReg, VRegMeta> meta;
    for (auto& [vreg, type] : func.params)
        meta[vreg].type = type;

    vector<int> call_indices;    // instruction indices of all CallC/CallPln
    vector<int> divmod_indices;  // instruction indices of all Div/Mod (%rax/%rdx clobbered)
    map<string, int> label_idx;  // label name → instruction index (for loop detection)

    for (int i = 0; i < (int)func.instrs.size(); i++) {
        auto& instr = func.instrs[i];

        auto setDef = [&](VReg dst, VRegType type) {
            auto& m = meta[dst];
            m.def_idx = i;
            if (m.first_def < 0) m.first_def = i;
            m.type    = type;
            m.writes.push_back(i);
            m.accesses.push_back(i);
        };
        auto addUse = [&](VReg vreg) {
            meta[vreg].last_any_use = max(meta[vreg].last_any_use, i);
            meta[vreg].accesses.push_back(i);
        };
        auto setBinOp = [&](VReg dst, VReg lhs, VReg rhs, VRegType type) {
            setDef(dst, type);
            addUse(lhs);
            addUse(rhs);
        };
        // argRegs is the integer/pointer arg register table for this call kind
        // (CallC/CallPln use phys.intArgs, CallSys uses phys.syscallArgs -- they
        // differ at slot 4: %rcx vs %r10). A slot that lands on an x86-emitter
        // scratch register (phys.scratch) cannot be bound directly either, since
        // other instructions between this def and the call may clobber it --
        // it falls through to addUse, same as an out-of-table slot.
        auto addCallArgs = [&](const vector<VReg>& args, const vector<string>& argRegs,
                               const vector<StructArgSpan>& spans) {
            vector<VRegType> types;
            for (auto vr : args)
                types.push_back(meta.count(vr) ? meta[vr].type : VRegType::Int64);
            vector<int> slots = placeCallArgs(types, spans, (int)argRegs.size(), (int)phys.floatArgs.size());
            for (size_t k = 0; k < args.size(); k++) {
                VReg vr = args[k];
                // Float args go to XMM registers, not intArgs. Track only liveness.
                bool in_table = !isFloat(types[k]) && slots[k] >= 0;
                bool is_scratch = in_table && find(phys.scratch.begin(), phys.scratch.end(),
                                                     argRegs[slots[k]]) != phys.scratch.end();
                if (in_table && !is_scratch)
                    meta[vr].call_uses.push_back({i, argRegs[slots[k]]});
                else
                    addUse(vr);
            }
        };

        std::visit(overloaded{
            [&](const LeaLabel& l)  { setDef(l.dst, l.type); },
            [&](const MovImm& m)    { setDef(m.dst, m.type); },
            [&](const InitVar& v)   { setDef(v.dst, v.type); meta[v.dst].isVar = true; },
            [&](const InitVarF& v)  { setDef(v.dst, v.type); meta[v.dst].isVar = true; },
            [&](const DeclVar& v)   { setDef(v.dst, v.type); meta[v.dst].isVar = true; },
            [&](const Add& a)      { setBinOp(a.dst, a.lhs, a.rhs, a.type); },
            [&](const Sub& s)      { setBinOp(s.dst, s.lhs, s.rhs, s.type); },
            [&](const Mul& m)      { setBinOp(m.dst, m.lhs, m.rhs, m.type); },
            [&](const Div& d)      { divmod_indices.push_back(i); setBinOp(d.dst, d.lhs, d.rhs, d.type); },
            [&](const Mod& m)      { divmod_indices.push_back(i); setBinOp(m.dst, m.lhs, m.rhs, m.type); },
            [&](const Neg& n)      { setDef(n.dst, n.type); addUse(n.src); },
            [&](const BitAnd& a)   { setBinOp(a.dst, a.lhs, a.rhs, a.type); },
            [&](const BitOr& o)    { setBinOp(o.dst, o.lhs, o.rhs, o.type); },
            [&](const BitXor& x)   { setBinOp(x.dst, x.lhs, x.rhs, x.type); },
            [&](const BitNot& n)   { setDef(n.dst, n.type); addUse(n.src); },
            [&](const Shift& s)    { setBinOp(s.dst, s.lhs, s.rhs, s.type); },
            [&](const ShiftImm& s) { setDef(s.dst, s.type); addUse(s.src); },
            [&](const PackF32x2& p) { setDef(p.dst, VRegType::Float64); addUse(p.lo); addUse(p.hi); },
            [&](const Cmp& c)      { setDef(c.dst, VRegType::Uint8); addUse(c.lhs); addUse(c.rhs); },
            [&](const Convert& c)  { setDef(c.dst, c.to); addUse(c.src); },
            [&](const CallC& c) {
                call_indices.push_back(i);
                if (c.callee) addUse(*c.callee);
                addCallArgs(c.args, phys.intArgs, c.structArgs);
                for (int k = 0; k < (int)c.dsts.size(); k++)
                    setDef(c.dsts[k], c.retTypes[k]);
            },
            [&](const CallPln& c) {
                call_indices.push_back(i);
                addCallArgs(c.args, phys.intArgs, {});
                for (int k = 0; k < (int)c.dsts.size(); k++)
                    setDef(c.dsts[k], c.retTypes[k]);
            },
            // Included in call_indices like CallC/CallPln (conservative: the
            // real syscall instruction only clobbers rax/rcx/r11, but treating
            // it as a full caller-saved clobber avoids threading a second
            // clobber set through the four call_indices consumers below --
            // see IT-3206 status notes for the tradeoff).
            [&](const CallSys& c) {
                call_indices.push_back(i);
                addCallArgs(c.args, phys.syscallArgs, {});
                for (int k = 0; k < (int)c.dsts.size(); k++)
                    setDef(c.dsts[k], c.retTypes[k]);
            },
            [&](const RetPln& r) {
                for (VReg vr : r.rets) {
                    addUse(vr);
                    meta[vr].isRetValue = true;
                }
            },
            [&](const CondJmp& cj) { addUse(cj.cond); },
            [&](const Label& lbl)  { label_idx[lbl.name] = i; },
            // Mov copies src into dst (the variable's canonical VReg). The write
            // keeps dst live too: a register freed before a later Mov into dst
            // could be handed to another value the Mov would clobber.
            [&](const Mov& mv)        {
                addUse(mv.src);
                addUse(mv.dst);
                meta[mv.dst].writes.push_back(i);
            },
            [&](const Copy& c)        { setDef(c.dst, c.type); addUse(c.src); },
            [&](const DerefLoadIdx& dl)  { setDef(dl.dst, dl.type); addUse(dl.base); addUse(dl.idx); },
            [&](const DerefStoreIdx& ds) { addUse(ds.base); addUse(ds.idx); addUse(ds.src); },
            [&](const CalcAddrIdx& ca)   { setDef(ca.dst, VRegType::Ptr64); addUse(ca.base); addUse(ca.idx); },
            [&](const DerefLoad&  dl)    { setDef(dl.dst, dl.type); addUse(dl.ptr); },
            [&](const DerefStore& ds)    { addUse(ds.ptr); addUse(ds.src); },
            [&](const CalcAddr&   ca)    { setDef(ca.dst, VRegType::Ptr64); addUse(ca.ptr); },
            // Taking a local's address forces it to a stable stack slot for the
            // rest of its lifetime, exactly like an InitVar-declared variable —
            // reuse the isVar mechanism rather than a second forced-stack path.
            [&](const LeaLocal&   ll)    {
                setDef(ll.dst, VRegType::Ptr64);
                meta[ll.local].isVar = meta[ll.local].addrTaken = true;
                addUse(ll.local);
            },
            [&](const FrameAddr&  fa)    { setDef(fa.dst, VRegType::Ptr64); },
            [&](const BlockEnter&) {},
            [&](const BlockLeave&) {},
            [&](const Jmp&)        {},
        }, instr);
    }

    // Collect loop regions: backward jumps define [loop_start_label_idx, jmp_idx].
    vector<pair<int,int>> loop_regions;
    for (int i = 0; i < (int)func.instrs.size(); i++) {
        if (auto* j = get_if<Jmp>(&func.instrs[i])) {
            auto it = label_idx.find(j->label);
            if (it != label_idx.end() && it->second < i)
                loop_regions.push_back({it->second, i});
        }
    }

    // A value defined before a loop and used inside it is re-read on the next
    // iteration, so it must survive until the backward jump, not just its last
    // textual use. Repeat until stable so nested loops extend to the outermost.
    for (auto& [vreg, m] : meta) {
        m.live_end = m.last_any_use;
        for (auto& [ci, pos] : m.call_uses) m.live_end = max(m.live_end, ci);
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& [ls, le] : loop_regions) {
                if (m.def_idx < ls && ls <= m.live_end && m.live_end < le) {
                    m.live_end = le;
                    changed = true;
                }
            }
        }
    }

    // Static estimate of how often an instruction runs: x10 per enclosing loop.
    vector<double> weight(func.instrs.size(), 1.0);
    for (auto& [ls, le] : loop_regions)
        for (int i = ls; i <= le; i++) weight[i] *= 10;

    // Step 2: Assign physical registers or stack slots.
    RegMap result;

    int callee_idx  = 0;
    int stack_bytes = 0;  // bytes consumed below %rbp so far

    static auto sizeOfType = [](VRegType type) -> int {
        switch (type) {
            case VRegType::Int8:   return 1;
            case VRegType::Int16:  return 2;
            case VRegType::Int32:  return 4;
            case VRegType::Float32: return 4;
            case VRegType::Float80: return 16;
            default:               return 8;  // Int64, Ptr64, Float64
        }
    };

    static auto alignUp = [](int v, int align) {
        return (v + align - 1) / align * align;
    };

    auto allocStackSlot = [&](VReg vreg, VRegType type) {
        int size    = sizeOfType(type);
        stack_bytes = alignUp(stack_bytes, size) + size;
        result[vreg] = PhysLoc{"", type, -stack_bytes}; // LCOV_EXCL_BR_LINE
    };

    // For temporary VRegs (non-variable, non-return-value, no call args): reuse an expired
    // stack slot of the same size if one exists, otherwise allocate a fresh slot.
    auto allocTempStackSlot = [&](VReg vreg, VRegType type, int def_idx) {
        int sz = sizeOfType(type);
        // Build the maximum lifetime-end per local stack slot of matching size.
        // A slot may be shared by multiple VRegs (due to earlier reuse), so we
        // must ensure ALL current occupants have expired before reusing the slot.
        map<int, int> slot_end;  // stackOffset → max vr_end of all VRegs at that offset
        for (auto& [vr, loc] : result) {
            if (!loc.isStack() || loc.stackOffset >= 0) continue;  // skip non-local
            if (sizeOfType(meta[vr].type) != sz) continue;
            int e = meta[vr].live_end;
            auto [it, inserted] = slot_end.emplace(loc.stackOffset, e);
            if (!inserted) it->second = std::max(it->second, e);
        }
        for (auto& [offset, max_end] : slot_end) {
            if (max_end < def_idx) {
                result[vreg] = PhysLoc{"", type, offset}; // LCOV_EXCL_BR_LINE
                return;
            }
        }
        allocStackSlot(vreg, type);
    };

    auto allocCalleeSavedOrStack = [&](VReg vreg, VRegType type) {
        // Float VRegs cannot use integer callee-saved registers; always spill to stack.
        bool is_flt = isFloat(type) || isX87(type);
        if (!is_flt && callee_idx < (int)phys.calleeSaved.size()) {
            result[vreg] = PhysLoc{phys.calleeSaved[callee_idx++], type}; // LCOV_EXCL_BR_LINE
        } else {
            const VRegMeta& vm = meta[vreg];
            if (!vm.isVar && !vm.isRetValue && vm.call_uses.empty()) {
                allocTempStackSlot(vreg, type, vm.def_idx);
            } else {
                allocStackSlot(vreg, type);
            }
        }
    };

    // Pre-bind parameters to argument registers, each class in its own
    // sequence (SysV), overflow of either class to the caller's stack in order.
    // If a parameter's live range crosses a call, move it to a callee-saved
    // register so it survives the call (caller-saved arg regs are clobbered).
    vector<tuple<string, VReg, VRegType>> pendingParamCopies;
    map<VReg, string> floatParamRegs;  // float params arriving in XMM arg regs
    vector<VRegType> paramTypes;
    for (auto& [vreg, type] : func.params) paramTypes.push_back(type);
    vector<int> paramSlots = placeCallArgs(paramTypes, {}, (int)phys.intArgs.size(), (int)phys.floatArgs.size());
    int paramStackBytes;
    vector<int> paramOffsets = placeStackArgs(paramTypes, {}, paramSlots, paramStackBytes);
    for (size_t pi = 0; pi < func.params.size(); pi++) {
        auto& [vreg, type] = func.params[pi];
        if (paramSlots[pi] < 0) {
            // Stack parameter: passed by caller above %rbp, from 16(%rbp).
            // The caller's stack is stable across all calls within this function.
            result[vreg] = PhysLoc{"", type, 16 + paramOffsets[pi]}; // LCOV_EXCL_BR_LINE
            continue;
        }
        if (isFloat(type)) {
            floatParamRegs[vreg] = phys.floatArgs[paramSlots[pi]];
            continue;
        }
        const string& argReg = phys.intArgs[paramSlots[pi]];

        const VRegMeta& m = meta[vreg];
        bool crosses_call = false;
        for (int c_idx : call_indices) {
            if (c_idx < m.live_end) {
                crosses_call = true;
                break;
            }
        }

        if (crosses_call) {
            allocCalleeSavedOrStack(vreg, type);
            pendingParamCopies.push_back({argReg, vreg, type});
        } else {
            result[vreg] = PhysLoc{argReg, type}; // LCOV_EXCL_BR_LINE
        }
    }

    // Float VRegs take XMM registers where estimated cheaper than the stack:
    // the stack costs every access; a register costs, for a value living across
    // a call (XMM registers are all caller-saved), a store to its home slot
    // after each write and a reload after each such call. Linear scan; when
    // registers run short, the values saving the most keep them.
    map<int, vector<VReg>> storesAfter, reloadsAfter;
    vector<tuple<string, VReg, VRegType>> homeParamCopies;
    {
        struct Cand { VReg vr; int start, end; double gain; vector<int> crossed; int reg = -1; };
        vector<Cand> cands;
        auto paramToStack = [&](VReg vr) {
            allocStackSlot(vr, meta[vr].type);
            pendingParamCopies.push_back({floatParamRegs[vr], vr, meta[vr].type});
        };
        for (auto& [vreg, m] : meta) {
            if (!isFloat(m.type) || result.count(vreg)) continue;
            bool is_param = floatParamRegs.count(vreg);
            int start = is_param ? -1 : m.first_def;
            if (m.addrTaken || m.live_end <= start) {
                if (is_param && m.live_end > start) paramToStack(vreg);
                continue;
            }
            double stack_cost = 0, reg_cost = 0;
            for (int a : m.accesses) stack_cost += weight[a];
            vector<int> crossed;
            for (int c : call_indices)
                if (start < c && c < m.live_end) crossed.push_back(c);
            if (!crossed.empty()) {
                for (int w : m.writes) reg_cost += weight[w];
                if (is_param) reg_cost += 1;
                for (int c : crossed) reg_cost += weight[c];
            }
            if (reg_cost < stack_cost)
                cands.push_back({vreg, start, m.live_end, stack_cost - reg_cost, crossed});
            else if (is_param)
                paramToStack(vreg);
        }
        stable_sort(cands.begin(), cands.end(),
                    [](const Cand& a, const Cand& b) { return a.start < b.start; });
        vector<int> owner(phys.floatAlloc.size(), -1);  // cands index holding each reg
        for (int k = 0; k < (int)cands.size(); k++) {
            int pick = -1;
            for (int r = 0; r < (int)owner.size(); r++) {
                // Strictly before: an operand dying at this instruction is still read by it.
                if (owner[r] >= 0 && cands[owner[r]].end < cands[k].start) owner[r] = -1;
                if (owner[r] < 0 && pick < 0) pick = r;
            }
            if (pick < 0) {
                for (int r = 0; r < (int)owner.size(); r++)
                    if (pick < 0 || cands[owner[r]].gain < cands[owner[pick]].gain) pick = r;
                if (pick < 0 || cands[owner[pick]].gain >= cands[k].gain) continue;
                cands[owner[pick]].reg = -1;
            }
            owner[pick] = k;
            cands[k].reg = pick;
        }
        for (auto& c : cands) {
            const VRegMeta& m = meta[c.vr];
            auto pit = floatParamRegs.find(c.vr);
            if (c.reg < 0) {
                if (pit != floatParamRegs.end()) paramToStack(c.vr);
                continue;  // a variable goes to Pass B, a temp to the stack below
            }
            PhysLoc loc{phys.floatAlloc[c.reg], m.type};
            if (!c.crossed.empty()) {
                int size = sizeOfType(m.type);
                stack_bytes = alignUp(stack_bytes, size) + size;
                loc.homeOffset = -stack_bytes;
                for (int w : m.writes) storesAfter[w].push_back(c.vr);
                for (int ci : c.crossed) reloadsAfter[ci].push_back(c.vr);
            }
            result[c.vr] = loc;
            if (pit != floatParamRegs.end()) {
                pendingParamCopies.push_back({pit->second, c.vr, m.type});
                if (loc.homeOffset)
                    homeParamCopies.push_back({pit->second, c.vr, m.type});
            }
        }
    }

    // Count how many vregs are return values (to detect multi-return)
    int numRetValues = 0;
    for (auto& [vreg, m] : meta)
        if (m.isRetValue) numRetValues++;

    for (auto& [vreg, m] : meta) {
        if (result.count(vreg)) continue;  // already bound (e.g., param pre-binding)
        if (m.isVar) continue;  // Pass B always allocates isVar to stack

        if (m.call_uses.empty()) {
            bool is_flt = isFloat(m.type) || isX87(m.type);
            if (m.isRetValue && numRetValues == 1 && !is_flt) {
                // Besides calls, most x86 emitters use %rax as scratch for stack
                // operands, so %rax is safe only if nothing that emits code lies
                // between the def and the return.
                bool rax_safe = true;
                for (int k = m.def_idx + 1; k < m.live_end; k++) {
                    auto& in = func.instrs[k];
                    if (!holds_alternative<Label>(in) && !holds_alternative<BlockEnter>(in)
                            && !holds_alternative<BlockLeave>(in)) {
                        rax_safe = false;
                        break;
                    }
                }
                if (rax_safe) {
                    result[vreg] = PhysLoc{"%rax", m.type}; // LCOV_EXCL_BR_LINE
                } else {
                    allocCalleeSavedOrStack(vreg, m.type);
                }
            } else if (m.isRetValue) {
                // Multi-return value: use callee-saved so each survives until RetPln.
                allocCalleeSavedOrStack(vreg, m.type);
            } else if (m.last_any_use >= 0) {
                // Used only as Add operand (not directly by any call)
                allocCalleeSavedOrStack(vreg, m.type);
            }
            // else: dead — no allocation needed
            continue;
        }

        // Has call uses — determine whether callee-saved is needed.
        bool needs_callee_saved = false;
        if (m.call_uses.size() > 1) {
            needs_callee_saved = true;
        } else {
            int use_idx = m.call_uses.front().first;
            for (int c_idx : call_indices) {
                if (m.def_idx < c_idx && c_idx < use_idx) {
                    needs_callee_saved = true;
                    break;
                }
            }
            // Direct assignment to the call's argument register only lives up to
            // that call: the call itself clobbers all caller-saved registers,
            // including the very one this vreg would occupy. If the vreg is used
            // again afterward (e.g. a struct pointer passed as an out-param, then
            // dereferenced to read a field the call just wrote), that later read
            // would see whatever the call left behind rather than the original
            // value, so it must survive in a callee-saved register instead.
            if (!needs_callee_saved && m.live_end > use_idx) {
                needs_callee_saved = true;
            }
        }

        if (needs_callee_saved) {
            allocCalleeSavedOrStack(vreg, m.type);
        } else {
            // If another vreg mapped to the desired register is still live at this vreg's
            // definition point, we'd clobber it.  Detect this and use callee-saved instead.
            // (e.g. Mod dst and the rhs param both want %rsi, but rhs is still live at Mod.)
            const string& desired = m.call_uses.front().second;
            bool conflict = false;
            for (auto& [vr, loc] : result) {
                if (loc.isStack() || loc.base != desired) continue;
                // Check if vr is live at m.def_idx.
                // Params have def_idx=-1 (never set by an instruction); treat them as
                // defined at 0 (live from function entry).
                const VRegMeta& um = meta[vr];
                int u_def = (um.def_idx < 0) ? 0 : um.def_idx;
                if (u_def > m.def_idx) continue;
                int u_last = um.live_end;
                // Strict ">": if u_last == m.def_idx the existing VReg's last use is the
                // same instruction as m's definition (e.g. call args die when the call
                // produces its result), so no real conflict.
                if (u_last > m.def_idx) { conflict = true; break; }
            }
            // %rax and %rdx are clobbered by every Div/Mod instruction (idivq uses
            // %rdx:%rax as dividend and overwrites both).  If this vreg's live range
            // spans any Div/Mod, avoid assigning it to those registers.
            if (!conflict && (desired == "%rdx" || desired == "%rax")) {
                int live_start = m.def_idx < 0 ? 0 : m.def_idx;
                int live_end   = m.call_uses.front().first;
                for (int dm : divmod_indices) {
                    if (dm > live_start && dm < live_end) {
                        conflict = true;
                        break;
                    }
                }
            }
            if (conflict) {
                allocCalleeSavedOrStack(vreg, m.type);
            } else {
                result[vreg] = PhysLoc{desired, m.type}; // LCOV_EXCL_BR_LINE
            }
        }
    }

    // Pass B: assign stack slots to isVar VRegs in instruction-definition order,
    // reusing freed slots via freePool. isVar is set either by InitVar/InitVarF
    // (literal-initialized locals) or by LeaLocal (address-taken locals,
    // regardless of how they were initialized) -- both funnel through this same
    // forced-stack path rather than each needing their own mechanism.
    {
        map<int, vector<int>> freePool;  // size(bytes) -> available offsets

        // Group isVar VRegs by the instruction index that defines them, so they're
        // allocated in the same order they're created, matching the freePool
        // release ordering driven by BlockLeave.expiredVars below.
        map<int, vector<VReg>> varsByDefIdx;
        for (auto& [vreg, m] : meta)
            if (m.isVar && m.def_idx >= 0)
                varsByDefIdx[m.def_idx].push_back(vreg);

        auto allocInitVar = [&](VReg dst, VRegType type) {
            if (result.count(dst)) return;  // already handled in Pass A
            int sz     = sizeOfType(type);
            auto& pool = freePool[sz];
            if (!pool.empty()) {
                int off = pool.back();
                pool.pop_back();
                result[dst] = PhysLoc{"", type, off}; // LCOV_EXCL_BR_LINE
            } else {
                allocStackSlot(dst, type);
            }
        };

        for (int i = 0; i < (int)func.instrs.size(); i++) {
            auto it = varsByDefIdx.find(i);
            if (it != varsByDefIdx.end())
                for (VReg vr : it->second)
                    allocInitVar(vr, meta[vr].type);
            if (auto* bl = std::get_if<BlockLeave>(&func.instrs[i])) {
                for (VReg vr : bl->expiredVars) {
                    if (result.count(vr) && result[vr].isStack())
                        freePool[sizeOfType(result[vr].type)].push_back(result[vr].stackOffset);
                }
            }
        }
    }

    // Reserve stack slots at the top of the frame to save any callee-saved
    // registers that were allocated.  Shift existing stack slot offsets down to
    // make room, then include the save area in stack_bytes.
    int k = callee_idx;
    if (k > 0) {
        int save_area = k * 8;
        for (auto& [vreg, loc] : result) {
            if (loc.isStack() && loc.stackOffset < 0)
                loc.stackOffset -= save_area;
            if (loc.homeOffset < 0)
                loc.homeOffset -= save_area;
        }
        stack_bytes += save_area;
    }

    // Frame blocks go below everything already placed, so the callee-saved
    // shift above cannot break their alignment; %rbp itself is 16-byte aligned.
    vector<int> frameBlockOffsets;
    for (auto& b : func.frameBlocks) {
        assert(b.align > 0 && b.align <= 16 && 16 % b.align == 0);
        int s = stack_bytes + b.size;
        while (s % b.align != 0) s++;
        frameBlockOffsets.push_back(-s);
        stack_bytes = s;
    }

    // Align frame size for ABI compliance: call pushes the 8-byte return
    // address, then pushq %rbp restores 16-byte alignment, so frameSize must
    // be a multiple of 16.
    int frameSize = alignUp(stack_bytes, 16);

    vector<string> usedCallee;
    for (int i = 0; i < k; i++)
        usedCallee.push_back(phys.calleeSaved[i]);

    vector<RegAllocResult::ParamCopy> paramCopies;
    for (auto& [argReg, vreg, ptype] : pendingParamCopies)
        paramCopies.push_back({argReg, result[vreg], ptype});
    for (auto& [argReg, vreg, ptype] : homeParamCopies)
        paramCopies.push_back({argReg, PhysLoc{"", ptype, result[vreg].homeOffset}, ptype});

    return {result, frameSize, usedCallee, paramCopies, frameBlockOffsets, storesAfter, reloadsAfter};
} // LCOV_EXCL_BR_LINE
