#include "InstQueryPerformanceFrequency.h"

std::map<CallContextKey, InstQueryPerformanceFrequencyArgs> InstQueryPerformanceFrequency::argsMap;
ADDRINT InstQueryPerformanceFrequency::nextCallId = 0;
std::ofstream* InstQueryPerformanceFrequency::outStream;

VOID InstQueryPerformanceFrequency::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpFrequency) {
    InstQueryPerformanceFrequencyArgs args;
    args.lpFrequency = lpFrequency;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstQueryPerformanceFrequency::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpFrequency) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        *outStream << std::endl << "[+] QueryPerformanceFrequency..." << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstQueryPerformanceFrequency::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpFrequency
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpFrequency
        IARG_END);

    RTN_Close(rtn);
}
