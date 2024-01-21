#include "InstGetModuleFileNameA.h"

std::map<CallContextKey, InstGetModuleFileNameAArgs> InstGetModuleFileNameA::argsMap;
ADDRINT InstGetModuleFileNameA::nextCallId = 0;

VOID InstGetModuleFileNameA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameAArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleFileNameA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetModuleFileNameAArgs& args = it->second;
        std::string ansiStringLpFilename = ConvertAddrToAnsiString(args.lpFilename);
        std::cout << "[Contradef] GetModuleFileNameA invocada com lpFilename: " << ansiStringLpFilename << std::endl;
        std::cout << "Valor de retorno de GetModuleFileNameA: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpara entrada após o uso
    }
}

VOID InstGetModuleFileNameA::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
        IARG_END);

    RTN_Close(rtn);

}