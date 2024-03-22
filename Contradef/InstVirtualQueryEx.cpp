#include "InstVirtualQueryEx.h"

std::map<CallContextKey, InstVirtualQueryExArgs> InstVirtualQueryEx::argsMap;
ADDRINT InstVirtualQueryEx::nextCallId = 0;
std::ofstream* InstVirtualQueryEx::outStream;

VOID InstVirtualQueryEx::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength) {
    InstVirtualQueryExArgs args;
    args.hProcess = hProcess;
    args.lpAddress = lpAddress;
    args.lpBuffer = lpBuffer;
    args.dwLength = dwLength;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

// Baseado em https://stackoverflow.com/questions/65897402/c-winapi-virtualqueryex-function-gives-me-000000
unsigned long InstVirtualQueryEx::show_module(MEMORY_BASIC_INFORMATION info) {
    unsigned long usage = 0;

    *outStream << "    Informação do bloco de memória: " << info.BaseAddress << "(" << info.RegionSize / 1024 << ")\t";

    switch (info.State) {
    case MEM_COMMIT:
        *outStream << "Committed";
        break;
    case MEM_RESERVE:
        *outStream << "Reserved";
        break;
    case MEM_FREE:
        *outStream << "Free";
        break;
    }
    *outStream << "\t";
    switch (info.Type) {
    case MEM_IMAGE:
        *outStream << "Code Module";
        break;
    case MEM_MAPPED:
        *outStream << "Mapped     ";
        break;
    case MEM_PRIVATE:
        *outStream << "Private    ";
    }
    *outStream << "\t";

    int guard = 0, nocache = 0;

    if (info.AllocationProtect & PAGE_NOCACHE)
        nocache = 1;
    if (info.AllocationProtect & PAGE_GUARD)
        guard = 1;

    info.AllocationProtect &= ~(PAGE_GUARD | PAGE_NOCACHE);

    if ((info.State == MEM_COMMIT) && (info.AllocationProtect == PAGE_READWRITE || info.AllocationProtect == PAGE_READONLY))
        usage += info.RegionSize;

    switch (info.AllocationProtect) {
    case PAGE_READONLY:
        *outStream << "Read Only";
        break;
    case PAGE_READWRITE:
        *outStream << "Read/Write";
        break;
    case PAGE_WRITECOPY:
        *outStream << "Copy on Write";
        break;
    case PAGE_EXECUTE:
        *outStream << "Execute only";
        break;
    case PAGE_EXECUTE_READ:
        *outStream << "Execute/Read";
        break;
    case PAGE_EXECUTE_READWRITE:
        *outStream << "Execute/Read/Write";
        break;
    case PAGE_EXECUTE_WRITECOPY:
        *outStream << "COW Executable";
        break;
    }

    if (guard)
        *outStream << "\tguard page";
    if (nocache)
        *outStream << "\tnon-cacheable";
    *outStream << "\n";
    return usage;
}

VOID InstVirtualQueryEx::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstVirtualQueryExArgs& args = it->second;
        *outStream << std::endl << "[+] VirtualQueryEx..." << std::endl;
        *outStream << "    hProcess (parâmetro): " << args.hProcess << std::endl;
        *outStream << "    lpAddress (parâmetro): " << args.lpAddress << std::endl;
        *outStream << "    lpBuffer (parâmetro): " << args.lpBuffer << std::endl;
        *outStream << "    dwLength (parâmetro): " << args.dwLength << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        MEMORY_BASIC_INFORMATION* info = reinterpret_cast<MEMORY_BASIC_INFORMATION*>(args.lpBuffer);
        show_module(*info);
        *outStream << "[*] Concluído" << std::endl << std::endl;

        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstVirtualQueryEx::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpBuffer
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwLength
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpBuffer
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwLength
        IARG_END);

    RTN_Close(rtn);
}
