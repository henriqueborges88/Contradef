#include "InstOutputDebugStringW.h"

std::map<CallContextKey, InstOutputDebugStringWArgs> InstOutputDebugStringW::argsMap;
ADDRINT InstOutputDebugStringW::nextCallId = 0;

VOID InstOutputDebugStringW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpOutputString) {
    InstOutputDebugStringWArgs args;
    args.lpOutputString = lpOutputString;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstOutputDebugStringW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpOutputString) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstOutputDebugStringWArgs& args = it->second;
        std::wstring wslpOutputString = ConvertAddrToWideString(args.lpOutputString);
        wprintf(L"[Contradef] OutputDebugStringW foi chamada com lpOutputString: %ls\n", wslpOutputString.c_str());
        std::cout << "[Contradef] Valor de retorno de OutputDebugStringW: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstOutputDebugStringW::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
        IARG_END);

    RTN_Close(rtn);
}
