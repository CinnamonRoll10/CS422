/*
 * Copyright (C) 2007-2023 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <vector>

#include "pin.H"

struct InsCount {
    UINT64 numLoads = 0;
    UINT64 numStores = 0;
    UINT64 numNops = 0;
    UINT64 numDirectCalls = 0;
    UINT64 numIndirectCalls = 0;
    UINT64 numReturns = 0;
    UINT64 numUncondBranches = 0;
    UINT64 numCondBranches = 0;
    UINT64 numLogicalOps = 0;
    UINT64 numRotateShift = 0;
    UINT64 numFlagOps = 0;
    UINT64 numVector = 0;
    UINT64 numCondMoves = 0;
    UINT64 numMMXSSE = 0;
    UINT64 numSysCalls = 0;
    UINT64 numFP = 0;
    UINT64 numRest = 0;
};

struct FootprintData {
    std::unordered_set<UINT64> uniqueInsChunks;
    std::unordered_set<UINT64> uniqueDataChunks;
    UINT64 insSingleChunk = 0;
    UINT64 insMultiChunk = 0;
    UINT64 dataSingleChunk = 0;
    UINT64 dataMultiChunk = 0;
};

struct InsChunkCntBool {
    std::unordered_set<UINT64> chunks;
    UINT64 singleChunk = 0;
    UINT64 multiChunk = 0;
    bool appeared = false;
};

struct iaProperties {
    std::vector<UINT64> insLength;
    std::vector<UINT64> opNumber;
    UINT64 regreadOp = 0;
    UINT64 regwriteOp = 0;
    UINT64 mem3Op = 0;       // only true predicate
    UINT64 mem1readOp = 0;   // only true predicate
    UINT64 mem2writeOp = 0;  // only true predicate
    UINT64 totalBytes = 0;
    UINT64 maxBytes = 0;
    UINT64 sumBytes = 0;
    UINT64 numMemInstr = 0;
    bool hasMemOp = false;
    INT32 max_imm = std::numeric_limits<int32_t>::min();
    INT32 min_imm = std::numeric_limits<int32_t>::max();
    ADDRDELTA min_disp = std::numeric_limits<ADDRDELTA>::max();
    ADDRDELTA max_disp = std::numeric_limits<ADDRDELTA>::min();
    iaProperties() {
        insLength.resize(20, 0);
        opNumber.resize(20, 0);
    }
};

/* ================================================================== */
// Global variables
/* ================================================================== */
InsCount PartA;  // Struct for PartA of HW1
FootprintData PartC;
iaProperties PartD;
UINT64 insCount = 0;     // number of dynamically executed instructions
UINT64 bblCount = 0;     // number of dynamically executed basic blocks
UINT64 threadCount = 0;  // total number of threads, including main thread
UINT64 fast_forward_count = 0;  // fast forward count
std::ostream *out = &std::cerr;
static std::ofstream *outFile = nullptr;
/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
                                 "count.out", "Output file name");

KNOB<BOOL> KnobCount(
    KNOB_MODE_WRITEONCE, "pintool", "count", "1",
    "count instructions, basic blocks and threads in the application");

KNOB<UINT64> KnobFastForward(
    KNOB_MODE_WRITEONCE, "pintool", "f", "0",
    "Number of billions of instructions to fast-forward before measuring");

/* ================================================================== */
// Global Structs Functions
/* ================================================================== */
VOID count_nops() { PartA.numNops++; }
VOID count_directcall() { PartA.numDirectCalls++; }
VOID count_indirectcall() { PartA.numIndirectCalls++; }
VOID count_returns() { PartA.numReturns++; }
VOID count_uncondbranches() { PartA.numUncondBranches++; }
VOID count_condbranches() { PartA.numCondBranches++; }
VOID count_logicalops() { PartA.numLogicalOps++; }
VOID count_rotateshift() { PartA.numRotateShift++; }
VOID count_flagops() { PartA.numFlagOps++; }
VOID count_numvector() { PartA.numVector++; }
VOID count_numcondmoves() { PartA.numCondMoves++; }
VOID count_mmxsse() { PartA.numMMXSSE++; }
VOID count_numsyscalls() { PartA.numSysCalls++; }
VOID count_fp() { PartA.numFP++; }
VOID count_rest() { PartA.numRest++; }
VOID count_loads(UINT64 x) { PartA.numLoads += x; }
VOID count_store(UINT64 x) { PartA.numStores += x; }

VOID doInsCount() { insCount++; }

/* ===================================================================== */
// Utilities
/* ===================================================================== */

INT32 Usage() {
    std::cerr << "This tool prints out the number of dynamically executed "
              << std::endl
              << "instructions, basic blocks and threads in the application."
              << std::endl
              << std::endl;

    std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

ADDRINT FastForward(void) {
    return ((insCount >= fast_forward_count) &&
            (insCount < fast_forward_count + 1000000000));
}

ADDRINT Terminate(void) {
    return (insCount >= fast_forward_count + 1000000000);
}

VOID MyExitRoutine() {
    UINT64 total = PartA.numLoads + PartA.numStores + PartA.numNops +
                   PartA.numDirectCalls + PartA.numIndirectCalls +
                   PartA.numReturns + PartA.numUncondBranches +
                   PartA.numCondBranches + PartA.numLogicalOps +
                   PartA.numRotateShift + PartA.numFlagOps + PartA.numVector +
                   PartA.numCondMoves + PartA.numMMXSSE + PartA.numSysCalls +
                   PartA.numFP + PartA.numRest;
    if (total == 0) total = 1;

    // ===================== PART 1 =====================
    *out << "\n╔══════════════════════════════════════════════════════════════╗"
            "\n";
    *out
        << "║             PART 1: DYNAMIC INSTRUCTION PROFILE              ║\n";
    *out
        << "╚══════════════════════════════════════════════════════════════╝\n";
    *out << "  Fast-Forward Count : " << fast_forward_count << "\n";
    *out << "  Total Instructions : " << insCount << "\n\n";

    *out << std::left << std::setw(5) << "#" << std::setw(25) << "Type"
         << std::setw(15) << "Count" << std::setw(10) << "Percent"
         << "\n";
    *out << std::string(55, '-') << "\n";

    auto printRow = [&](int num, std::string name, UINT64 count) {
        *out << std::left << std::setw(5) << num << std::setw(25) << name
             << std::setw(15) << count << std::fixed << std::setprecision(3)
             << (100.0 * count / total) << "%\n";
    };

    printRow(1, "Loads", PartA.numLoads);
    printRow(2, "Stores", PartA.numStores);
    printRow(3, "NOPs", PartA.numNops);
    printRow(4, "Direct Calls", PartA.numDirectCalls);
    printRow(5, "Indirect Calls", PartA.numIndirectCalls);
    printRow(6, "Returns", PartA.numReturns);
    printRow(7, "Uncond Branches", PartA.numUncondBranches);
    printRow(8, "Cond Branches", PartA.numCondBranches);
    printRow(9, "Logical Ops", PartA.numLogicalOps);
    printRow(10, "Rotate & Shift", PartA.numRotateShift);
    printRow(11, "Flag Ops", PartA.numFlagOps);
    printRow(12, "Vector (AVX)", PartA.numVector);
    printRow(13, "Cond Moves", PartA.numCondMoves);
    printRow(14, "MMX/SSE", PartA.numMMXSSE);
    printRow(15, "System Calls", PartA.numSysCalls);
    printRow(16, "Floating Point", PartA.numFP);
    printRow(17, "The Rest", PartA.numRest);
    *out << std::string(55, '-') << "\n";
    *out << std::left << std::setw(30) << "     Total " << total << "\n";

    // ===================== PART 2 =====================
    UINT64 categoryASum =
        PartA.numNops + PartA.numDirectCalls + PartA.numIndirectCalls +
        PartA.numReturns + PartA.numUncondBranches + PartA.numCondBranches +
        PartA.numLogicalOps + PartA.numRotateShift + PartA.numFlagOps +
        PartA.numVector + PartA.numCondMoves + PartA.numMMXSSE +
        PartA.numSysCalls + PartA.numFP + PartA.numRest;
    UINT64 totalInstructions = PartA.numLoads + PartA.numStores + categoryASum;
    UINT64 totalCycles =
        (PartA.numLoads * 70) + (PartA.numStores * 70) + (categoryASum * 1);
    if (totalInstructions == 0) totalInstructions = 1;
    double cpi = (double)totalCycles / (double)totalInstructions;

    *out << "\n╔══════════════════════════════════════════════════════════════╗"
            "\n";
    *out
        << "║                   PART 2: CPI ANALYSIS                       ║\n";
    *out
        << "╚══════════════════════════════════════════════════════════════╝\n";
    *out << std::left;
    *out << "  " << std::setw(28) << "Total Cycles:" << totalCycles << "\n";
    *out << "  " << std::setw(28) << "Total Instructions:" << totalInstructions
         << "\n";
    *out << "  " << std::setw(28) << "CPI:" << std::fixed
         << std::setprecision(4) << cpi << "\n";

    // ===================== PART 3 =====================
    *out << "\n╔══════════════════════════════════════════════════════════════╗"
            "\n";
    *out
        << "║             PART 3: MEMORY FOOTPRINT                         ║\n";
    *out
        << "╚══════════════════════════════════════════════════════════════╝\n";
    *out << std::left;
    *out << "  " << std::setw(38)
         << "Unique 32-byte instruction chunks:" << PartC.uniqueInsChunks.size()
         << "\n";
    *out << "  " << std::setw(38)
         << "Unique 32-byte data chunks:" << PartC.uniqueDataChunks.size()
         << "\n\n";
    *out << "  Instruction chunks  [ single : multi ] = "
         << PartC.insSingleChunk << " : " << PartC.insMultiChunk << "\n";
    *out << "  Data chunks         [ single : multi ] = "
         << PartC.dataSingleChunk << " : " << PartC.dataMultiChunk << "\n";

    // ===================== PART 4 =====================
    *out << "\n╔══════════════════════════════════════════════════════════════╗"
            "\n";
    *out
        << "║               PART 4: ia32 ISA PROPERTIES                    ║\n";
    *out
        << "╚══════════════════════════════════════════════════════════════╝\n";

    *out << "\n  [ Instruction Length Distribution ]\n";
    *out << "  " << std::setw(10) << "Length" << std::setw(15) << "Count"
         << "\n";
    *out << "  " << std::string(23, '-') << "\n";
    for (size_t i = 0; i < PartD.insLength.size(); i++) {
        if (PartD.insLength[i] > 0)
            *out << "  " << std::setw(10) << i << std::setw(15)
                 << PartD.insLength[i] << "\n";
    }

    *out << "\n  [ Operand Count Distribution ]\n";
    *out << "  " << std::setw(10) << "Operands" << std::setw(15) << "Count"
         << "\n";
    *out << "  " << std::string(23, '-') << "\n";
    for (size_t i = 0; i < PartD.opNumber.size(); i++) {
        if (PartD.opNumber[i] > 0)
            *out << "  " << std::setw(10) << i << std::setw(15)
                 << PartD.opNumber[i] << "\n";
    }

    *out << "\n  [ Register & Memory Operand Counts ]\n";
    *out << "  " << std::string(40, '-') << "\n";
    *out << "  " << std::setw(35)
         << "2 register read operands:" << PartD.regreadOp << "\n";
    *out << "  " << std::setw(35)
         << "1 register write operand:" << PartD.regwriteOp << "\n";
    *out << "  " << std::setw(35) << "3 memory operands:" << PartD.mem3Op
         << "\n";
    *out << "  " << std::setw(35)
         << "1 memory read operand:" << PartD.mem1readOp << "\n";
    *out << "  " << std::setw(35)
         << "2 memory write operands:" << PartD.mem2writeOp << "\n";

    *out << "\n  [ Immediate Values ]\n";
    *out << "  " << std::string(40, '-') << "\n";
    *out << "  " << std::setw(15) << "Min:" << PartD.min_imm << "\n";
    *out << "  " << std::setw(15) << "Max:" << PartD.max_imm << "\n";

    *out << "\n  [ Displacement Values ]\n";
    *out << "  " << std::string(40, '-') << "\n";
    *out << "  " << std::setw(15) << "Min:" << PartD.min_disp << "\n";
    *out << "  " << std::setw(15) << "Max:" << PartD.max_disp << "\n";

    double avg = (PartD.numMemInstr == 0)
                     ? 0.0
                     : (double)PartD.sumBytes / (double)PartD.numMemInstr;
    *out << "\n  [ Memory Bytes Touched ]\n";
    *out << "  " << std::string(40, '-') << "\n";
    *out << "  " << std::setw(15) << "Max bytes:" << PartD.maxBytes << "\n";
    *out << "  " << std::setw(15) << "Avg bytes:" << std::fixed
         << std::setprecision(4) << avg << "\n";

    *out << "\n╚══════════════════════════════════════════════════════════════╝"
            "\n"
            "\n";

    if (outFile) {
        outFile->close();
        delete outFile;
        outFile = nullptr;
    }
    exit(0);
}
VOID InsChunks(InsChunkCntBool *blockchunkcntbool) {
    PartC.insSingleChunk += blockchunkcntbool->singleChunk;
    PartC.insMultiChunk += blockchunkcntbool->multiChunk;

    if (blockchunkcntbool->appeared == false) {
        blockchunkcntbool->appeared = true;
        for (auto it : blockchunkcntbool->chunks) {
            PartC.uniqueInsChunks.insert(it);
        }
    }
}

VOID RecordDataAddr(ADDRINT addr, UINT32 size) {
    UINT64 startID = addr >> 5;
    UINT64 endID = (addr + size - 1) >> 5;
    for (UINT64 chunk = startID; chunk <= endID; chunk++) {
        PartC.uniqueDataChunks.insert(chunk);
    }
    if (startID != endID) {
        PartC.dataMultiChunk++;
    } else {
        PartC.dataSingleChunk++;
    }
}

VOID iaPropPred(iaProperties *insiaProp) {
    PartD.mem1readOp += insiaProp->mem1readOp;
    PartD.mem2writeOp += insiaProp->mem2writeOp;
    PartD.mem3Op += insiaProp->mem3Op;
    if (insiaProp->hasMemOp) {
        PartD.numMemInstr++;
        PartD.sumBytes += insiaProp->totalBytes;
        if (insiaProp->totalBytes > PartD.maxBytes) {
            PartD.maxBytes = insiaProp->totalBytes;
        }
        if (insiaProp->min_disp < PartD.min_disp)
            PartD.min_disp = insiaProp->min_disp;
        if (insiaProp->max_disp > PartD.max_disp)
            PartD.max_disp = insiaProp->max_disp;
    }
}

VOID iaProp(iaProperties *blockiaProp) {
    for (UINT32 i = 0; i < blockiaProp->insLength.size(); i++) {
        if (i >= PartD.insLength.size()) PartD.insLength.resize(i + 1, 0);
        PartD.insLength[i] += blockiaProp->insLength[i];
    }
    for (UINT32 i = 0; i < blockiaProp->opNumber.size(); i++) {
        if (i >= PartD.opNumber.size()) PartD.opNumber.resize(i + 1, 0);
        PartD.opNumber[i] += blockiaProp->opNumber[i];
    }
    PartD.regreadOp += blockiaProp->regreadOp;
    PartD.regwriteOp += blockiaProp->regwriteOp;
    if (blockiaProp->max_imm > PartD.max_imm)
        PartD.max_imm = blockiaProp->max_imm;
    if (blockiaProp->min_imm < PartD.min_imm)
        PartD.min_imm = blockiaProp->min_imm;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Trace(TRACE trace, VOID *v) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)MyExitRoutine,
                           IARG_END);

        InsChunkCntBool *blockchunkcntbool = new InsChunkCntBool();
        iaProperties *blockiaProp = new iaProperties();

        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)doInsCount, IARG_END);
            /* Part A */
            UINT32 memopcnt = INS_MemoryOperandCount(ins);
            for (UINT32 memOp = 0; memOp < memopcnt; memOp++) {
                UINT32 datasize = INS_MemoryOperandSize(ins, memOp);
                UINT64 operations = datasize / 4 + (datasize % 4 != 0);
                if (INS_MemoryOperandIsRead(ins, memOp)) {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                     IARG_END);
                    INS_InsertThenPredicatedCall(
                        ins, IPOINT_BEFORE, (AFUNPTR)count_loads, IARG_UINT64,
                        operations, IARG_END);
                }
                if (INS_MemoryOperandIsWritten(ins, memOp)) {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                     IARG_END);
                    INS_InsertThenPredicatedCall(
                        ins, IPOINT_BEFORE, (AFUNPTR)count_store, IARG_UINT64,
                        operations, IARG_END);
                }
            }
            UINT32 cat = INS_Category(ins);

            if (cat == XED_CATEGORY_NOP) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_nops, IARG_END);
            } else if (cat == XED_CATEGORY_CALL) {
                if (INS_IsDirectCall(ins)) {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                     IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                                 (AFUNPTR)count_directcall,
                                                 IARG_END);
                } else {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                     IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                                 (AFUNPTR)count_indirectcall,
                                                 IARG_END);
                }

            } else if (cat == XED_CATEGORY_RET) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_returns, IARG_END);
            } else if (cat == XED_CATEGORY_UNCOND_BR) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_uncondbranches,
                                             IARG_END);
            } else if (cat == XED_CATEGORY_COND_BR) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_condbranches, IARG_END);
            } else if (cat == XED_CATEGORY_LOGICAL) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_logicalops, IARG_END);
            } else if (cat == XED_CATEGORY_ROTATE ||
                       cat == XED_CATEGORY_SHIFT) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_rotateshift, IARG_END);
            } else if (cat == XED_CATEGORY_FLAGOP) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_flagops, IARG_END);
            } else if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2 ||
                       cat == XED_CATEGORY_AVX2GATHER ||
                       cat == XED_CATEGORY_AVX512) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_numvector, IARG_END);
            } else if (cat == XED_CATEGORY_CMOV) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_numcondmoves, IARG_END);
            } else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_mmxsse, IARG_END);
            } else if (cat == XED_CATEGORY_SYSCALL) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)count_numsyscalls, IARG_END);
            } else if (cat == XED_CATEGORY_X87_ALU) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_fp, IARG_END);
            } else {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                             (AFUNPTR)count_rest, IARG_END);
            }

            /* Part C */
            ADDRINT ins_Addr = INS_Address(ins);
            UINT32 ins_size = INS_Size(ins);
            UINT64 start_id = ins_Addr >> 5;
            UINT64 end_id = (ins_Addr + ins_size - 1) >> 5;
            blockchunkcntbool->chunks.insert(start_id);
            if (end_id == start_id) {
                blockchunkcntbool->singleChunk++;

            } else {
                blockchunkcntbool->multiChunk++;
                blockchunkcntbool->chunks.insert(end_id);
            }
            for (UINT32 memOp = 0; memOp < memopcnt; memOp++) {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                                 IARG_END);
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)RecordDataAddr,
                    IARG_MEMORYOP_EA, memOp, IARG_MEMORYOP_SIZE, memOp,
                    IARG_END);
            }

            /* Part D */
            /* (a) Instruction length */
            UINT32 ins_len = INS_Size(ins);
            if (ins_len >= blockiaProp->insLength.size()) {
                blockiaProp->insLength.resize(ins_len + 1, 0);
            }
            blockiaProp->insLength[ins_len]++;

            /* (b) Number of operands */
            UINT32 op_num = INS_OperandCount(ins);
            if (op_num >= blockiaProp->opNumber.size()) {
                blockiaProp->opNumber.resize(op_num + 1, 0);
            }

            blockiaProp->opNumber[op_num]++;

            /* (c) Register Read Operations */
            UINT32 readreg = INS_MaxNumRRegs(ins);
            if (readreg == 2) {
                blockiaProp->regreadOp++;
            }

            /* (d) Register Write Operations */
            UINT32 writereg = INS_MaxNumWRegs(ins);
            if (writereg == 1) {
                blockiaProp->regwriteOp++;
            }
            /* (i) Maximum and Minimum value */
            for (UINT32 imm = 0; imm < op_num; imm++) {
                if (INS_OperandIsImmediate(ins, imm)) {
                    INT32 opImm = INS_OperandImmediate(ins, imm);
                    blockiaProp->max_imm = (blockiaProp->max_imm > opImm)
                                               ? blockiaProp->max_imm
                                               : opImm;
                    blockiaProp->min_imm = (blockiaProp->min_imm < opImm)
                                               ? blockiaProp->min_imm
                                               : opImm;
                }
            }

            // Predicate Check Instructions must be instrumented
            // instruction wise

            iaProperties *insiaProp = new iaProperties();
            /* (e) True predicate 3 memory operand */
            /* (f) 1 memory read (g) 2 memory write */
            UINT32 d_e_Cnt_read = 0, d_e_Cnt_write = 0;
            for (UINT32 memOp = 0; memOp < memopcnt; memOp++) {
                if (INS_MemoryOperandIsRead(ins, memOp)) {
                    d_e_Cnt_read++;
                }
                if (INS_MemoryOperandIsWritten(ins, memOp)) {
                    d_e_Cnt_write++;
                }
            }
            if ((d_e_Cnt_read + d_e_Cnt_write) == 3) {
                insiaProp->mem3Op++;
            }
            if (d_e_Cnt_read == 1) {
                insiaProp->mem1readOp++;
            }
            if (d_e_Cnt_write == 2) {
                insiaProp->mem2writeOp++;
            }

            /* (h) True predicate maximum and average size */
            UINT64 totalBytesThisIns = 0;
            for (UINT32 memop = 0; memop < memopcnt; memop++) {
                totalBytesThisIns += INS_MemoryOperandSize(ins, memop);
            }
            insiaProp->totalBytes = totalBytesThisIns;
            insiaProp->hasMemOp = (memopcnt > 0);

            /* (j) Predicated displacement */
            for (UINT32 op = 0; op < op_num; op++) {
                if (INS_OperandIsMemory(ins, op)) {
                    ADDRDELTA disp = INS_OperandMemoryDisplacement(ins, op);
                    if (disp < insiaProp->min_disp) insiaProp->min_disp = disp;
                    if (disp > insiaProp->max_disp) insiaProp->max_disp = disp;
                }
            }

            /* (D) part predicate call */
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward,
                             IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE,
                                         (AFUNPTR)iaPropPred, IARG_PTR,
                                         insiaProp, IARG_END);
        }

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsChunks, IARG_PTR,
                           blockchunkcntbool, IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)iaProp, IARG_PTR,
                           blockiaProp, IARG_END);
    }
}

VOID ThreadStart(THREADID threadIndex, CONTEXT *ctxt, INT32 flags, VOID *v) {
    threadCount++;
}

VOID Fini(INT32 code, VOID *v) { MyExitRoutine(); }

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    fast_forward_count = KnobFastForward.Value() * 1000000000ULL;

    std::string fileName = KnobOutputFile.Value();

    if (!fileName.empty()) {
        outFile = new std::ofstream(fileName.c_str());
        out = outFile;
    }

    if (KnobCount.Value()) {
        TRACE_AddInstrumentFunction(Trace, 0);
        PIN_AddThreadStartFunction(ThreadStart, 0);
        PIN_AddFiniFunction(Fini, 0);
    }

    std::cerr << "\n╔══════════════════════════════════════════════════════════"
                 "═══╗\n";
    std::cerr
        << "║                  CS422 HW1 - PIN Tool                       ║\n";
    std::cerr
        << "╠═════════════════════════════════════════════════════════════╣\n";
    std::cerr << "║  Fast-forward  : " << std::left << std::setw(43)
              << fast_forward_count << "║\n";
    std::cerr << "║  Measure       : " << std::left << std::setw(43)
              << "1,000,000,000 instructions"
              << "║\n";
    std::cerr << "║  Output file   : " << std::left << std::setw(43)
              << (KnobOutputFile.Value().empty() ? "stderr"
                                                 : KnobOutputFile.Value())
              << "║\n";
    std::cerr << "╚════════════════════════════════════════════════════════════"
                 "═╝\n\n";

    PIN_StartProgram();

    return 0;
}