#include "InstGetWindowTextA.h"

std::map<CallContextKey, InstGetWindowTextAArgs> InstGetWindowTextA::argsMap;
ADDRINT InstGetWindowTextA::nextCallId = 0;
std::ofstream* InstGetWindowTextA::outStream;

VOID InstGetWindowTextA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {
    InstGetWindowTextAArgs args;
    args.hWnd = hWnd;
    args.lpString = lpString;
    args.nMaxCount = nMaxCount;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetWindowTextA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetWindowTextAArgs& args = it->second;
        std::string ansiStringlpString = ConvertAddrToAnsiString(args.lpString);
        *outStream << std::endl << "[+] GetWindowTextA..." << std::endl;
        *outStream << "    lpString (parámetro): " << ansiStringlpString << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetWindowTextA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
        IARG_END);

    RTN_Close(rtn);
}
