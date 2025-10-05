// Este módulo foi implementado usando como base a Pintool DebugTrace, disponível no SDK do Pin. 

#include "TraceInstructions.h"
#include "InstrumentationUtils.h"

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

BOOL alignIdent = false;

enum OPCLASS : UINT32 { OP_IMM = 0, OP_REG = 1, OP_MEM = 2 };

PIN_MUTEX TraceInstructions::fileOutMutex;

std::ofstream TraceInstructions::out;
// Buffer de 512 KB para out
const std::size_t TraceInstructions::bufferSize = 1024 * 1024 * 10;
std::vector<char> TraceInstructions::buffer(bufferSize);  // o buffer precisa durar enquanto o ofstream estiver aberto

INT32 TraceInstructions::enabled = 0;
const UINT32 TraceInstructions::MaxEmitArgs = 4;
AFUNPTR TraceInstructions::emitFuns[] = { AFUNPTR(TraceInstructions::EmitNoValues), AFUNPTR(TraceInstructions::Emit1Values), AFUNPTR(TraceInstructions::Emit2Values), AFUNPTR(TraceInstructions::Emit3Values), AFUNPTR(TraceInstructions::Emit4Values) };
std::stack<ADDRINT> TraceInstructions::callStack;

std::string GetOpVal(INT32 valismemaddr, ADDRINT val, UINT32 size) {
    std::string valnum;
    std::string valpre;

    if (valismemaddr)
    {
        valpre = " = " + GetNumericHexValue(val, 8);

        std::string valnumon = GetNumericValueFromRef(reinterpret_cast<void*>(val), size);
        if (!valnumon.empty()) {
            valnum = " -> " + valnumon;
        }

        std::string valstr = FormatPtrOrValue(val);
        if (!valstr.empty()) {
            valnum += " -> \"" + valstr + "\"";
        }
    }
    else
    {
        valpre = " = " + GetNumericHexValue(val, size);
        std::string tmp = GetStringValueFromRegister(val, size);
        valnum = " (" + GetNumericValue(val, size) + ")";
        if (!tmp.empty()) valnum += " -> \"" + tmp + "\"";
    }

    return valpre + valnum;
}

BOOL TraceInstructions::Emit(THREADID threadid) {
    if (!enabled || KnobSilent || (KnobWatchThread != static_cast<THREADID>(-1) && KnobWatchThread != threadid)) return false;
    return true;
}

VOID TraceInstructions::Flush() {
    if (KnobFlush) out << std::flush;
}

VOID TraceInstructions::EmitNoValues(THREADID threadid, std::string* str) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << threadid << std::hex << "]" << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::Emit1Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << threadid << std::hex << "] " << *reg1str << " = " << reg1val << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::Emit2Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << threadid << std::hex << "] " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::Emit3Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val, std::string* reg3str, ADDRINT reg3val) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << threadid << std::hex << "] " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << ", " << *reg3str << " = " << reg3val << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::Emit4Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val, std::string* reg3str, ADDRINT reg3val, std::string* reg4str, ADDRINT reg4val) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << threadid << std::hex << "] " << *reg1str << " = " << reg1val << ", " << *reg2str << " = " << reg2val << ", " << *reg3str << " = " << reg3val << ", " << *reg4str << " = " << reg4val << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::EmitXMM(THREADID threadid, UINT32 regno, PINTOOL_REGISTER* xmm) {
    if (!Emit(threadid)) return;
    PIN_MutexLock(&fileOutMutex);
    out << "\t\t\tXMM" << std::dec << regno << " := " << std::setfill('0') << std::hex;
    out.unsetf(std::ios::showbase);
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 8 || i == 12) out << "_";
        out << std::setw(2) << static_cast<int>(xmm->byte[15 - i]);
    }
    out << std::setfill(' ') << '\n';
    out.setf(std::ios::showbase);
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::AddXMMEmit(INS ins, IPOINT point, REG xmm_dst) {
    INS_InsertCall(ins, point, AFUNPTR(EmitXMM), IARG_THREAD_ID, IARG_UINT32, xmm_dst - REG_XMM0, IARG_REG_CONST_REFERENCE, xmm_dst, IARG_END);
}


VOID TraceInstructions::EmitCmpValues(ADDRINT addr, THREADID tid, std::string* str, std::string* reg1str, INT32 reg1ismem, UINT32 reg1size, ADDRINT reg1val, std::string* reg2str, INT32 reg2ismem, UINT32 reg2size, ADDRINT reg2val, std::string* reg3str, ADDRINT reg3val)     
{
    if (!Emit(tid)) return;

    std::string valnum1, valnum2;
    // tratamento do operando 1
    if (reg1ismem) {
        valnum1 += " [" + Hex(reg1val, reg1size) + "]";
    }
    valnum1 += GetOpVal(reg1ismem, reg1val, reg1size);

    // tratamento do operando 2 (mesma lógica) 
    if (reg2ismem) {
        valnum2 += " [" + Hex(reg2val, reg2size) + "]";
    }
    valnum2 = GetOpVal(reg2ismem, reg2val, reg2size);

    // impressão segura
    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << tid << std::hex << "] "
        << *reg1str << valnum1 << ", "
        << *reg2str << valnum2 << ", "
        << *reg3str << " = " << reg3val
        << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);

}


VOID TraceInstructions::AddEmitCmp(INS ins, IPOINT point, std::string& traceString, REG reg3val) {
    // Instrumenta a instrução CMP
    if (INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
        // CMP com dois registradores
        REG reg1 = INS_OperandReg(ins, 0);
        REG reg2 = INS_OperandReg(ins, 1);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitCmpValues,
            IARG_ADDRINT, INS_Address(ins),
            IARG_THREAD_ID, 
            IARG_PTR, new std::string(traceString),
            IARG_PTR, new std::string(REG_StringShort(reg1)),
            IARG_BOOL, false,
            IARG_ADDRINT, REG_Size(reg1),
            IARG_REG_VALUE, reg1,
            IARG_PTR, new std::string(REG_StringShort(reg2)),
            IARG_BOOL, false,
            IARG_ADDRINT, REG_Size(reg2),
            IARG_REG_VALUE, reg2,
            IARG_PTR, new std::string(REG_StringShort(reg3val)),
            IARG_REG_VALUE, reg3val,
            IARG_END);
    }
    else if (INS_OperandIsReg(ins, 0) && INS_OperandIsMemory(ins, 1)) {
        // CMP entre registrador e memória
        REG reg1 = INS_OperandReg(ins, 0);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitCmpValues,
            IARG_ADDRINT, INS_Address(ins),
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceString),
            IARG_PTR, new std::string(REG_StringShort(reg1)),
            IARG_BOOL, false,
            IARG_ADDRINT, REG_Size(reg1),
            IARG_REG_VALUE, reg1,
            IARG_PTR, new std::string("memval"),
            IARG_BOOL, true,
            IARG_MEMORYREAD_SIZE,
            IARG_MEMORYREAD_EA,
            IARG_PTR, new std::string(REG_StringShort(reg3val)),
            IARG_REG_VALUE, reg3val,
            IARG_END);
    }
    else if (INS_OperandIsMemory(ins, 0) && INS_OperandIsReg(ins, 1)) {
        // CMP entre memória e registrador
        REG reg2 = INS_OperandReg(ins, 1);
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitCmpValues,
            IARG_ADDRINT, INS_Address(ins),
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceString),
            IARG_PTR, new std::string("memval"),
            IARG_BOOL, true,
            IARG_MEMORYREAD_SIZE,
            IARG_MEMORYREAD_EA,
            IARG_PTR, new std::string(REG_StringShort(reg2)),
            IARG_BOOL, false,
            IARG_ADDRINT, REG_Size(reg2),
            IARG_REG_VALUE, reg2,
            IARG_PTR, new std::string(REG_StringShort(reg3val)),
            IARG_REG_VALUE, reg3val,
            IARG_END);
    }
    else if (INS_OperandIsImmediate(ins, 1)) {
        // CMP entre registrador/memória e imediato
        if (INS_OperandIsReg(ins, 0)) {
            REG reg1 = INS_OperandReg(ins, 0);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitCmpValues,
                IARG_ADDRINT, INS_Address(ins),                 // addr
                IARG_THREAD_ID,                                  // tid
                IARG_PTR, new std::string(traceString),     // *str

                // operando 1 (reg)
                IARG_PTR, new std::string(REG_StringShort(reg1)),
                IARG_UINT32, 0,                                // reg1IsMem = false
                IARG_UINT32, REG_Size(reg1),
                IARG_REG_VALUE, reg1,

                // operando 2 (imediato)/
                IARG_PTR, new std::string("immval"),
                IARG_UINT32, 0,                                // reg2IsMem = false
                IARG_UINT32, (INS_OperandWidth(ins, 1) / 8),
                IARG_ADDRINT, static_cast<ADDRINT>(INS_OperandImmediate(ins, 1)),

                // flags / terceiro reg 
                IARG_PTR, new std::string(REG_StringShort(reg3val)),
                IARG_REG_VALUE, reg3val,

                IARG_END);
        }
        else if (INS_OperandIsMemory(ins, 0)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitCmpValues,
                // dados fixos/
                IARG_ADDRINT, INS_Address(ins),          // addr
                IARG_THREAD_ID,                           // tid
                IARG_PTR, new std::string(traceString),

                // operando-1  (memória)
                IARG_PTR, new std::string("memval"),
                IARG_UINT32, 1,                         // reg1IsMem = true
                IARG_MEMORYREAD_SIZE,
                IARG_MEMORYREAD_EA,

                // operando-2  (imediato)
                IARG_PTR, new std::string("immval"),
                IARG_UINT32, 0,                         // reg2IsMem = false
                IARG_UINT32, (INS_OperandWidth(ins, 1) / 8),
                IARG_ADDRINT, static_cast<ADDRINT>(INS_OperandImmediate(ins, 1)),

                // registrador extra usado na função (ZF/CF, etc.)
                IARG_PTR, new std::string(REG_StringShort(reg3val)),
                IARG_REG_VALUE, reg3val,

                IARG_END);
        }
    }
}

VOID TraceInstructions::EmitMovValue(ADDRINT ip, THREADID tid, std::string* tag,
    REG   dstReg,     /* REG_INVALID() se destino é memória */
    ADDRINT ea,       /* endereço efetivo – 0 se não usado  */
    UINT64 val,       /* valor final movido                 */
    UINT32 size,      /* 1,2,4,8                            */
    BOOL  isWrite)    /* TRUE  se [mem] ← X                 */
{
    PIN_MutexLock(&fileOutMutex);

    out << *tag << " | [T" << std::dec << tid << std::hex << "] ";

    BOOL regismem = FALSE;

    if (!isWrite) {                       // reg ← [mem]
        out << REG_StringShort(dstReg);
        if (ea != 0) {
            regismem = TRUE;
        }
    }
    else if (dstReg == REG_INVALID()) {   // [mem] ← val
        regismem = TRUE;
        out << "memval [" << Hex(ea, size) << "]";
    }
    else {                                // mov reg,reg   ou  mov reg,imm
        out << REG_StringShort(dstReg);
    }
    std::string opVal = GetOpVal(regismem, val, size);
    out << opVal << '\n';
    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

struct MovMemInfo { ADDRINT ea; UINT32 size; REG dst; BOOL isWrite; };
static TLS_KEY tlsMov;

VOID TraceInstructions::MovBefore(THREADID tid, ADDRINT ea, UINT32 size, REG dst, BOOL isWrite)
{
    auto* inf = static_cast<MovMemInfo*>(PIN_GetThreadData(tlsMov, tid));
    inf->ea = ea;
    inf->size = size;
    inf->dst = dst;      /* REG_INVALID() se destino é memória      */
    inf->isWrite = isWrite;
}

VOID TraceInstructions::MovAfter(ADDRINT ip, THREADID tid, std::string* str, ADDRINT srcVal /*apenas p/ writes reg→mem ou imm→mem*/)
{
    auto* inf = static_cast<MovMemInfo*>(PIN_GetThreadData(tlsMov, tid));

    /* leitura final ─ para read ou confirmação de write */
    UINT64 val = 0;
    if (!inf->isWrite) {                              /* mov reg , [mem] */
        val = srcVal;                                 /* já está em reg  */
    }
    else {                                          /* [mem] = X       */
        PIN_SafeCopy(&val, reinterpret_cast<void*>(inf->ea), inf->size);
    }

    EmitMovValue(ip, tid, str,
        inf->dst,          /* REG destino  (REG_INVALID se mem) */
        inf->ea,           /* endereço de memória               */
        val,               /* valor final                       */
        inf->size,
        inf->isWrite);     /* para formatação                    */
}

VOID TraceInstructions::AddEmitMov(INS ins, std::string& traceStr)
{
    if (INS_Opcode(ins) != XED_ICLASS_MOV) return;

    /* --- caso 1: mov reg , [mem] ------------------------------------------- */
    if (INS_OperandIsReg(ins, 0) && INS_OperandIsMemory(ins, 1)) {
        REG dst = INS_OperandReg(ins, 0);
        UINT32 size = INS_MemoryReadSize(ins);

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MovBefore),
            IARG_THREAD_ID,
            IARG_MEMORYREAD_EA,
            IARG_UINT32, size,
            IARG_UINT32, dst,
            IARG_BOOL, FALSE,    /* não é write */
            IARG_END);

        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(MovAfter),
            IARG_INST_PTR, IARG_THREAD_ID,
            IARG_PTR, new std::string(traceStr),
            IARG_REG_VALUE, dst,      /* valor lido já está no reg */
            IARG_END);
        return;
    }

    /* --- caso 2: mov [mem] , reg ------------------------------------------- */
    if (INS_OperandIsMemory(ins, 0) && INS_OperandIsReg(ins, 1)) {
        REG src = INS_OperandReg(ins, 1);
        UINT32 size = INS_MemoryWriteSize(ins);

        /* BEFORE: guarda EA / size / dst inválido / é write */
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MovBefore),
            IARG_THREAD_ID,
            IARG_MEMORYWRITE_EA,
            IARG_UINT32, size,
            IARG_UINT32, REG_INVALID(),
            IARG_BOOL, TRUE,
            IARG_END);

        /* AFTER: valor do src (ainda no registrador)   */
        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(MovAfter),
            IARG_INST_PTR, IARG_THREAD_ID,
            IARG_PTR, new std::string(traceStr),
            IARG_REG_VALUE, src,
            IARG_END);
        return;
    }

    /* --- caso 3: mov [mem] , imm ------------------------------------------- */
    if (INS_OperandIsMemory(ins, 0) && INS_OperandIsImmediate(ins, 1)) {
        UINT32 size = INS_MemoryWriteSize(ins);
        ADDRINT imm = INS_OperandImmediate(ins, 1);

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MovBefore),
            IARG_THREAD_ID,
            IARG_MEMORYWRITE_EA,
            IARG_UINT32, size,
            IARG_UINT32, REG_INVALID(),
            IARG_BOOL, TRUE,
            IARG_END);

        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(MovAfter),
            IARG_INST_PTR, IARG_THREAD_ID,
            IARG_PTR, new std::string(traceStr),
            IARG_ADDRINT, imm,
            IARG_END);
        return;
    }

    /* -------- mov  reg , reg ---------------------------------*/
    if (INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1))
    {
        REG    dst = INS_OperandReg(ins, 0);          // destino
        UINT32 sz = REG_Size(dst);

        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(EmitMovValue),
            IARG_INST_PTR, IARG_THREAD_ID,
            IARG_PTR, new std::string(traceStr),
            IARG_UINT32, dst,          // dstReg
            IARG_ADDRINT, 0,           // ea = 0   (não há endereço de memória)
            IARG_REG_VALUE, dst,       // val final
            IARG_UINT32, sz,
            IARG_BOOL, FALSE,          // isWrite
            IARG_END);
    }

    /* -------- mov  reg , imm ---------------------------------*/
    else if (INS_OperandIsReg(ins, 0) && INS_OperandIsImmediate(ins, 1))
    {
        REG   dst = INS_OperandReg(ins, 0);
        UINT64 imm = static_cast<UINT64>(INS_OperandImmediate(ins, 1));
        UINT32 size = REG_Size(dst);

        /* AFTER já contém o valor correto no próprio registrador       */
        INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(EmitMovValue),
            IARG_INST_PTR,
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceStr),
            IARG_UINT32, dst,          // destino
            IARG_ADDRINT, 0,           // ea = 0   (não há endereço de memória)
            IARG_REG_VALUE, dst,       // ← lê o valor *atual* do reg
            IARG_UINT32, size,
            IARG_BOOL, FALSE,         // isWrite (mem) = false
            IARG_END);
    }
}

VOID TraceInstructions::AddEmitPush(ADDRINT ip, THREADID tid, std::string* str, std::string* regName, ADDRINT rspAfter, UINT32 size)
{
    std::string valreg;

    valreg += GetOpVal(TRUE, rspAfter, size);

    PIN_MutexLock(&fileOutMutex);
    out << *str << " | [T" << std::dec << tid << std::hex << "] " << *regName << valreg << '\n';
    PIN_MutexUnlock(&fileOutMutex);

}

VOID TraceInstructions::AddEmit(INS ins, IPOINT point, std::string& traceString, UINT32 regCount, REG regs[]) {
    if (regCount > MaxEmitArgs) regCount = MaxEmitArgs;

    IARGLIST args = IARGLIST_Alloc();
    for (UINT32 i = 0; i < regCount; i++) {
        IARGLIST_AddArguments(args, IARG_PTR, new std::string(REG_StringShort(regs[i])), IARG_REG_VALUE, regs[i], IARG_END);
    }
    
    auto opc = INS_Opcode(ins);

    std::string mnemonic = INS_Mnemonic(ins);
    if (opc == XED_ICLASS_CMP || opc == XED_ICLASS_TEST || mnemonic.rfind("CMOV", 0) == 0) {
        AddEmitCmp(ins, point, traceString, regs[0]);
    }
    else if (opc == XED_ICLASS_MOV ||
        opc == XED_ICLASS_MOVZX ||
        opc == XED_ICLASS_MOVSX)
    {
        AddEmitMov(ins, traceString);
    }
    else if (INS_Opcode(ins) == XED_ICLASS_PUSH) {
        UINT32 sz = INS_MemoryWriteSize(ins);     // 1 / 2 / 4 / 8 bytes

        INS_InsertCall(ins, IPOINT_AFTER,
            (AFUNPTR)AddEmitPush,
            IARG_INST_PTR,
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceString),
            IARG_PTR, new std::string(REG_StringShort(REG_FullRegName(INS_RegW(ins, 0)))),
            IARG_REG_VALUE, REG_STACK_PTR, // RSP depois do push
            IARG_UINT32, sz, // tamanho do dado empilhado
            IARG_END);
    }
    else {
        INS_InsertCall(ins, point, emitFuns[regCount], IARG_THREAD_ID, IARG_PTR, new std::string(traceString), IARG_IARGLIST, args, IARG_END);
    }
    IARGLIST_Free(args);
}

static VOID* WriteEa[PIN_MAX_THREADS];

VOID TraceInstructions::CaptureWriteEa(THREADID threadid, VOID* addr) {
    WriteEa[threadid] = addr;
}

VOID TraceInstructions::ShowN(UINT32 n, VOID* ea)
{
    out.unsetf(std::ios::showbase);
    // Print out the bytes in "big endian even though they are in memory little endian.
    // This is most natural for 8B and 16B quantities that show up most frequently.
    // The address pointed to
    out << std::setfill('0');
    UINT8 b[512];
    UINT8* x;
    if (n > 512)
        x = new UINT8[n];
    else
        x = b;
    PIN_SafeCopy(x, static_cast<UINT8*>(ea), n);
    for (UINT32 i = 0; i < n; i++)
    {
        out << std::setw(2) << static_cast<UINT32>(x[n - i - 1]);
        if (((reinterpret_cast<ADDRINT>(ea) + n - i - 1) & 0x3) == 0 && i < n - 1) out << "_";
    }
    out << std::setfill(' ');
    out.setf(std::ios::showbase);
    if (n > 512) delete[] x;
}

VOID TraceInstructions::EmitWrite(ADDRINT addr, THREADID threadid, std::string * str, UINT32 size)
{
    if (!Emit(threadid)) return;


    VOID* ea = WriteEa[threadid];

    if (!KnobTraceMemoryInstructions) return;

    PIN_MutexLock(&fileOutMutex);

    out << "                                 Write ";

    switch (size)
    {
    case 0:
        out << "0 repeat count" << '\n';
        break;

    case 1:
    {
        UINT8 x;
        PIN_SafeCopy(&x, static_cast<UINT8*>(ea), 1);
        out << "*(UINT8*)" << ea << " = " << static_cast<UINT32>(x) << '\n';
    }
    break;

    case 2:
    {
        UINT16 x;
        PIN_SafeCopy(&x, static_cast<UINT16*>(ea), 2);
        out << "*(UINT16*)" << ea << " = " << x << '\n';
    }
    break;

    case 4:
    {
        UINT32 x;
        PIN_SafeCopy(&x, static_cast<UINT32*>(ea), 4);
        out << "*(UINT32*)" << ea << " = " << x << '\n';
    }
    break;

    case 8:
    {
        UINT64 x;
        PIN_SafeCopy(&x, static_cast<UINT64*>(ea), 8);
        out << "*(UINT64*)" << ea << " = " << x << '\n';
    }
    break;

    default:
        out << "*(UINT" << std::dec << size * 8 << std::hex << ")" << ea << " = ";
        ShowN(size, ea);
        out << '\n';
        break;
    }

    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::EmitRead(ADDRINT addr, THREADID threadid, std::string* str, VOID* ea, UINT32 size)
{
    if (!Emit(threadid)) return;

    if (!KnobTraceMemoryInstructions) return;

    PIN_MutexLock(&fileOutMutex);

    out << "                                 Read ";

    switch (size)
    {
    case 0:
        out << "0 repeat count" << '\n';
        break;

    case 1:
    {
        UINT8 x;
        PIN_SafeCopy(&x, static_cast<UINT8*>(ea), 1);
        out << static_cast<UINT32>(x) << " = *(UINT8*)" << ea << '\n';
    }
    break;

    case 2:
    {
        UINT16 x;
        PIN_SafeCopy(&x, static_cast<UINT16*>(ea), 2);
        out << x << " = *(UINT16*)" << ea << '\n';
    }
    break;

    case 4:
    {
        UINT32 x;
        PIN_SafeCopy(&x, static_cast<UINT32*>(ea), 4);
        out << x << " = *(UINT32*)" << ea << '\n';
    }
    break;

    case 8:
    {
        UINT64 x;
        PIN_SafeCopy(&x, static_cast<UINT64*>(ea), 8);
        out << x << " = *(UINT64*)" << ea << '\n';
    }
    break;

    default:
        ShowN(size, ea);
        out << " = *(UINT" << std::dec << size * 8 << std::hex << ")" << ea << '\n';
        break;
    }

    Flush();
    PIN_MutexUnlock(&fileOutMutex);

}

static INT32 indent = 0;

VOID TraceInstructions::Indent()
{
    for (INT32 i = 0; i < indent; i++)
    {
        out << "| ";
    }
}

VOID TraceInstructions::EmitDirectCall(THREADID threadid, ADDRINT instAddress, string* str, INT32 tailCall, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
{
    if (!Emit(threadid)) return;

    if (tailCall)
    {
        // A tail call is like an implicit return followed by an immediate call
        indent--;
    }
    else
    {
        callStack.push(instAddress);
    }

    PIN_MutexLock(&fileOutMutex);
    Indent();

    out << *str << " [T" << std::dec << threadid << std::hex << "] (" << arg0 << ", " << arg1 << ", " << arg2 << ", " << arg3 << ", " << arg4 << ", " << arg5 << ", " << arg6 << ", ...)" << '\n';

    indent++;

    Flush();
    PIN_MutexUnlock(&fileOutMutex);

}

VOID TraceInstructions::EmitIndirectCall(THREADID threadid, ADDRINT instAddress, string* str, ADDRINT target, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
{
    if (!Emit(threadid)) return;

    if (target == 0) return;

    callStack.push(instAddress);

    PIN_MutexLock(&fileOutMutex);

    Indent();
    out << *str;

    PIN_LockClient();
    RTN rtnTarget = RTN_FindByAddress(target);
    PIN_UnlockClient();
    string s = FormatAddress(target, rtnTarget, KnobSymbols, KnobFullImgName, KnobLines);
    
    out << s << " [T" << std::dec << threadid << std::hex << "] (" << arg0 << ", " << arg1 << ", " << arg2 << ", " << arg3 << ", " << arg4 << ", " << arg5 << ", " << arg6 << ", ...)" << '\n';
    indent++;

    Flush();
    PIN_MutexUnlock(&fileOutMutex);
}

VOID TraceInstructions::EmitReturn(THREADID threadid, ADDRINT instAddress, string* str, ADDRINT retAddr, ADDRINT ret0)
{
    if (!Emit(threadid)) return;

    if (retAddr == 0) return;

    PIN_MutexLock(&fileOutMutex);

    indent--;
    if (indent < 0)
    {
        out << "@@@ return underflow\n";
        indent = 0;
    }

    Indent();

    ADDRINT callAddr = 0;
    if (!callStack.empty()) {
        callAddr = callStack.top();
        callStack.pop(); 
    }

    out << *str << " [T" << std::dec << threadid << std::hex << "] returns: " << ret0 << ", ret addr: " << retAddr << ", call addr: " << callAddr << '\n';

    Flush();
    PIN_MutexUnlock(&fileOutMutex);

}

BOOL NeedInstrument(TRACE trace, INS ins) {
    if (KnobTraceOnlyMain) {
        if (!IsMainExecutable(INS_Address(ins))) {
            RTN srcRtn = TRACE_Rtn(trace);
            if (RTN_Valid(srcRtn)) {
                ADDRINT delta = INS_Address(ins) - RTN_Address(srcRtn);
                if (delta > 12)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

VOID TraceInstructions::CallTrace(TRACE trace, INS ins)
{
    if (!KnobTraceCalls) return;

    if (INS_IsCall(ins) && !INS_IsDirectControlFlow(ins))
    {
        PIN_LockClient();
        RTN insRtn = RTN_FindByAddress(INS_Address(ins));
        PIN_UnlockClient();
        // Indirect call
        string s = "Call " + FormatAddress(INS_Address(ins), insRtn, KnobSymbols, KnobFullImgName, KnobLines);
        s += " -> ";

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitIndirectCall), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new string(s),
            IARG_BRANCH_TARGET_ADDR, IARG_FUNCARG_CALLSITE_VALUE, 0, IARG_FUNCARG_CALLSITE_VALUE, 1, IARG_FUNCARG_CALLSITE_VALUE, 2, IARG_FUNCARG_CALLSITE_VALUE, 3, IARG_FUNCARG_CALLSITE_VALUE, 4, IARG_FUNCARG_CALLSITE_VALUE, 5, IARG_FUNCARG_CALLSITE_VALUE, 6, IARG_END);
    }
    else if (INS_IsDirectControlFlow(ins))
    {
        PIN_LockClient();
        // Is this a tail call?
        RTN sourceRtn = TRACE_Rtn(trace);
        RTN destRtn = RTN_FindByAddress(INS_DirectControlFlowTargetAddress(ins));
        PIN_UnlockClient();

        if (INS_IsCall(ins)         // conventional call
            || sourceRtn != destRtn // tail call
            )
        {
            BOOL tailcall = !INS_IsCall(ins);

            string s = "";
            if (tailcall)
            {
                s += "Tailcall ";
            }
            else
            {
                if (INS_IsProcedureCall(ins))
                    s += "Call ";
                else
                {
                    s += "PcMaterialization ";
                    tailcall = 1;
                }
            }

            //s += INS_Mnemonic(ins) + " ";
            
            PIN_LockClient();
            RTN rtnIns = RTN_FindByAddress(INS_Address(ins));
            PIN_UnlockClient();
            s += FormatAddress(INS_Address(ins), rtnIns, KnobSymbols, KnobFullImgName, KnobLines);
            s += " -> ";

            ADDRINT target = INS_DirectControlFlowTargetAddress(ins);

            PIN_LockClient();
            RTN rtnTarget = RTN_FindByAddress(target);
            PIN_UnlockClient();
            s += FormatAddress(target, rtnTarget, KnobSymbols, KnobFullImgName, KnobLines);

            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitDirectCall), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new string(s), IARG_BOOL,
                tailcall, IARG_FUNCARG_CALLSITE_VALUE, 0, IARG_FUNCARG_CALLSITE_VALUE, 1, IARG_FUNCARG_CALLSITE_VALUE, 2, IARG_FUNCARG_CALLSITE_VALUE, 3, IARG_FUNCARG_CALLSITE_VALUE, 4, IARG_FUNCARG_CALLSITE_VALUE, 5, IARG_FUNCARG_CALLSITE_VALUE, 6, IARG_END);
        }
    }
    else if (INS_IsRet(ins))
    {
        RTN rtn = TRACE_Rtn(trace);

#if defined(TARGET_LINUX) && defined(TARGET_IA32)
        //        if( RTN_Name(rtn) ==  "_dl_debug_state") return;
        if (RTN_Valid(rtn) && RTN_Name(rtn) == "_dl_runtime_resolve") return;
#endif
        string tracestring = "Return " + FormatAddress(INS_Address(ins), rtn, KnobSymbols, KnobFullImgName, KnobLines);
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitReturn), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new string(tracestring),
            IARG_RETURN_IP, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
    }
}

VOID TraceInstructions::InstructionTrace(TRACE trace, INS ins)
{
    if (!NeedInstrument(trace, ins)) return;

    ADDRINT addr = INS_Address(ins);
    ASSERTX(addr);

    // Format the string at instrumentation time
    string traceString = "";
    PIN_LockClient();
    RTN rtnIns = RTN_FindByAddress(INS_Address(ins));
    PIN_UnlockClient();
    string astring = FormatAddress(INS_Address(ins), rtnIns, KnobSymbols, KnobFullImgName, KnobLines);
    for (INT32 length = astring.length(); length < 1; length++) // for (INT32 length = astring.length(); length < 95; length++)
    {
        traceString += " ";
    }
    traceString = astring + traceString;

    traceString += " | " + INS_Disassemble(ins);

    for (INT32 length = traceString.length(); length < 1; length++) // for (INT32 length = traceString.length(); length < 140; length++)
    {
        traceString += " ";
    }

    INT32 regCount = 0;
    REG regs[20];
    REG xmm_dst = REG_INVALID();

    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++)
    {
        REG x = REG_FullRegName(INS_RegW(ins, i));

        if (REG_is_gr(x)
#if defined(TARGET_IA32)
            || x == REG_EFLAGS
#elif defined(TARGET_IA32E)
            || x == REG_RFLAGS
#endif
            )
        {
            regs[regCount] = x;
            regCount++;
        }

        if (REG_is_xmm(x)) xmm_dst = x;
    }

    if (INS_IsValidForIpointAfter(ins))
    {
        AddEmit(ins, IPOINT_AFTER, traceString, regCount, regs);
    }
    if (INS_IsValidForIpointTakenBranch(ins))
    {
        AddEmit(ins, IPOINT_TAKEN_BRANCH, traceString, regCount, regs);
    }

    if (xmm_dst != REG_INVALID())
    {
        if (INS_IsValidForIpointAfter(ins)) AddXMMEmit(ins, IPOINT_AFTER, xmm_dst);
        if (INS_IsValidForIpointTakenBranch(ins)) AddXMMEmit(ins, IPOINT_TAKEN_BRANCH, xmm_dst);
    }
}


VOID TraceInstructions::Trace(TRACE trace, VOID* v)
{
    if (enabled)
    {
        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        {
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            {

                InstructionTrace(trace, ins);

                CallTrace(trace, ins);

            }
        }
    }
}


static void TraceInstructions::OnSig(THREADID threadIndex, CONTEXT_CHANGE_REASON reason, const CONTEXT* ctxtFrom, CONTEXT* ctxtTo, INT32 sig, VOID* v)
{
    PIN_MutexLock(&fileOutMutex);

    if (ctxtFrom != 0)
    {
        ADDRINT address = PIN_GetContextReg(ctxtFrom, REG_INST_PTR);
        out << "SIG signal=" << sig << " on thread " << threadIndex << " at address " << std::hex << address << std::dec << " ";
    }

    switch (reason)
    {
    case CONTEXT_CHANGE_REASON_FATALSIGNAL:
        out << "FATALSIG" << sig;
        break;
    case CONTEXT_CHANGE_REASON_SIGNAL:
        out << "SIGNAL " << sig;
        break;
    case CONTEXT_CHANGE_REASON_SIGRETURN:
        out << "SIGRET";
        break;

    case CONTEXT_CHANGE_REASON_APC:
        out << "APC";
        break;

    case CONTEXT_CHANGE_REASON_EXCEPTION:
        out << "EXCEPTION";
        break;

    case CONTEXT_CHANGE_REASON_CALLBACK:
        out << "CALLBACK";
        break;

    default:
        break;
    }
    out << '\n';
    PIN_MutexUnlock(&fileOutMutex);

}

VOID TraceInstructions::ThreadStart(THREADID tid, CONTEXT*, INT32, VOID*)
{
    PIN_SetThreadData(tlsMov, new MovMemInfo{}, tid);
}

void TraceInstructions::enable() {
    TraceInstructions::enabled = 1;
}
void TraceInstructions::disable() {
    TraceInstructions::enabled = 0;
}

VOID TraceInstructions::Fini(INT32 code, VOID* v)
{
    out << std::flush;
    out.close();
}

int TraceInstructions::Init(string pid, std::string filename)
{
    filename += "." + pid + ".TraceInstructions.cdf";
    out.open(filename.c_str(), std::ios::out | std::ios::binary);
    out << std::hex << std::right;
    out.setf(std::ios::showbase);
    out.rdbuf()->pubsetbuf(&buffer[0], bufferSize);

    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddContextChangeFunction(OnSig, 0);
    tlsMov = PIN_CreateThreadDataKey(nullptr);
    PIN_AddThreadStartFunction(ThreadStart, nullptr);
    PIN_AddFiniFunction(Fini, 0);

    enabled = 1;

    return 0;
}