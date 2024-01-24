#include "InstDebugActiveProcess.h"

std::map<CallContextKey, InstDebugActiveProcessArgs> InstDebugActiveProcess::argsMap;
ADDRINT InstDebugActiveProcess::nextCallId = 0;
std::ofstream* InstDebugActiveProcess::outStream;

VOID InstDebugActiveProcess::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT dwProcessId) {
    InstDebugActiveProcessArgs args;
    args.dwProcessId = dwProcessId;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstDebugActiveProcess::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT dwProcessId) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstDebugActiveProcessArgs& args = it->second;
        *outStream << std::endl << "[+] DebugActiveProcess..." << std::endl;
        *outStream << "    dwProcessId (parâmetro): " << args.dwProcessId << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstDebugActiveProcess::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwProcessId
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwProcessId
        IARG_END);

    RTN_Close(rtn);
}
