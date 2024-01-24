#include "InstGetModuleFileNameExA.h"

std::map<CallContextKey, InstGetModuleFileNameExAArgs> InstGetModuleFileNameExA::argsMap;
ADDRINT InstGetModuleFileNameExA::nextCallId = 0;
std::ofstream* InstGetModuleFileNameExA::outStream;

VOID InstGetModuleFileNameExA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameExAArgs args;
    args.hProcess = hProcess;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleFileNameExA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetModuleFileNameExAArgs& args = it->second;
        std::string ansiStringLpFilename = ConvertAddrToAnsiString(args.lpFilename);
        *outStream << std::endl << "[+] GetModuleFileNameExA..." << std::endl;
        *outStream << "    lpPerformanceCount (parámetro): " << lpFilename << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetModuleFileNameExA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
        IARG_END);

    RTN_Close(rtn);
}
