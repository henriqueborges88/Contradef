

#include "TraceMemory.h"

std::ofstream TraceMemory::memTraceOut;
// Buffer de 512 KB para memTraceOut
const std::size_t TraceMemory::bufferSize = 1024 * 1024 * 10;
std::vector<char> TraceMemory::buffer(bufferSize);  // o buffer precisa durar enquanto o ofstream estiver aberto

PIN_MUTEX TraceMemory::fileMemTraceOutMutex;

VOID* WriteEa[PIN_MAX_THREADS];

VOID TraceMemory::CaptureWriteEa(THREADID threadid, VOID* addr) {
    WriteEa[threadid] = addr;
}

VOID TraceMemory::WriteMemTraceOut(THREADID threadid, std::string* disasmStr, VOID* ea, UINT32 size)
{
    const ADDRINT address = reinterpret_cast<ADDRINT>(ea);

    const std::string numeric = GetNumericValueFromRef(ea, size);

    /* ---- tenta extrair string no endereço ----------------------- */
    std::string suffix;
    std::string extracted = FormatPtrOrValue(address, 256);

    if (!extracted.empty())
    {
        suffix = " -> \"" + extracted + "\"";
    }
    else if (!PIN_CheckReadAccess(ea))
    {   // sem acesso: mostra bytes como ASCII “forçado”
        suffix = " -> \"" + InterpretAddrIntAsASCII(address) + "\"";
    }

    std::string numVal = GetNumericHexValue(reinterpret_cast<UINT64>(ea), 8);

    PIN_MutexLock(&fileMemTraceOutMutex);
    memTraceOut
        << "[T" << std::dec << threadid << std::hex << "] "
        << *disasmStr << " ["
        << numVal << "] = "
        << numeric << suffix << "\n"
        << "\n";
    PIN_MutexUnlock(&fileMemTraceOutMutex);
}


VOID TraceMemory::EmitWrite(ADDRINT addr, THREADID threadid, std::string* str, UINT32 size)
{
    VOID* ea = WriteEa[threadid];

    WriteMemTraceOut(threadid, str, ea, size);
}

VOID TraceMemory::EmitRead(ADDRINT addr, THREADID threadid, std::string* str, VOID* ea, UINT32 size)
{
    WriteMemTraceOut(threadid, str, ea, size);
}


VOID TraceMemory::InstTraceMemory(INS ins, VOID* v)
{
    if (!KnobTraceMemory) return;

    ADDRINT addr = INS_Address(ins);

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(addr)) {
        return;
    }

    // Format the string at instrumentation time
    std::string traceString = "";
    std::string astring = FormatAddress(INS_Address(ins), RTN_FindByAddress(INS_Address(ins)));
    for (INT32 length = astring.length(); length < 30; length++)
    {
        traceString += " ";
    }
    traceString = astring + traceString;

    traceString += " | " + INS_Disassemble(ins);

    if (INS_IsMemoryWrite(ins)) // && INS_IsStandardMemop(ins)
    {
        traceString += " | [Write]:";

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CaptureWriteEa), IARG_THREAD_ID, IARG_MEMORYWRITE_EA, IARG_END);

        LEVEL_VM::IPOINT ipoint = LEVEL_VM::IPOINT::IPOINT_INVALID;
        if (INS_IsValidForIpointAfter(ins))
        {
            ipoint = IPOINT_AFTER;
        }
        if (INS_IsValidForIpointTakenBranch(ins))
        {
            ipoint = IPOINT_TAKEN_BRANCH;
        }

        if (ipoint != LEVEL_VM::IPOINT::IPOINT_INVALID) {
            INS_InsertCall(ins, ipoint, AFUNPTR(EmitWrite),
                IARG_ADDRINT, INS_Address(ins),
                IARG_THREAD_ID,
                IARG_PTR, new std::string(traceString),
                IARG_MEMORYWRITE_SIZE,
                IARG_END);
        }
    }

    if (INS_HasMemoryRead2(ins)) // && INS_IsStandardMemop(ins)
    {
        traceString += " | [Read 2 Op]:";

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitRead,
            IARG_ADDRINT, INS_Address(ins),
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceString),
            IARG_MEMORYREAD2_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_END);
    }

    if (INS_IsMemoryRead(ins) && !INS_IsPrefetch(ins)) // && INS_IsStandardMemop(ins)
    {
        traceString += " | [Read 1 Op]:";

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)EmitRead,
            IARG_ADDRINT, INS_Address(ins),
            IARG_THREAD_ID,
            IARG_PTR, new std::string(traceString),
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_END);
    }

}

VOID TraceMemory::Fini(INT32 code, VOID* v)
{
    memTraceOut << std::flush;
    memTraceOut.close();
}

int TraceMemory::Init(std::string pid, std::string filename)
{
    filename += "." + pid + ".TraceMemory.cdf";
    memTraceOut.open(filename.c_str(), std::ios::out | std::ios::binary);
    memTraceOut << std::hex << std::right;
    memTraceOut.setf(std::ios_base::showbase);
    memTraceOut.rdbuf()->pubsetbuf(&buffer[0], bufferSize);

    INS_AddInstrumentFunction(InstTraceMemory, 0);
    PIN_AddFiniFunction(Fini, 0);

    return 0;
}