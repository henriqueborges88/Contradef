#include "InstGetWindowTextW.h"

std::map<CallContextKey, InstGetWindowTextWArgs> InstGetWindowTextW::argsMap;
ADDRINT InstGetWindowTextW::nextCallId = 0;

VOID InstGetWindowTextW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {
    InstGetWindowTextWArgs args;
    args.hWnd = hWnd;
    args.lpString = lpString;
    args.nMaxCount = nMaxCount;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetWindowTextW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetWindowTextWArgs& args = it->second;
        std::wstring wslpString = ConvertAddrToWideString(args.lpString);
        wprintf(L"[Contradef] GetWindowTextW foi chamada com lpString: %ls\n", wslpString.c_str());

        std::cout << "[Contradef] GetWindowTextW completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetWindowTextW::InstrumentFunction(RTN rtn) {
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
