#include "InstRegOpenKeyW.h"

std::map<CallContextKey, InstRegOpenKeyWArgs> InstRegOpenKeyW::argsMap;
ADDRINT InstRegOpenKeyW::nextCallId = 0;

VOID InstRegOpenKeyW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    InstRegOpenKeyWArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegOpenKeyW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegOpenKeyWArgs& args = it->second;
        std::wstring wslpSubKey = ConvertAddrToWideString(args.lpSubKey);
        wprintf(L"[Contradef] RegOpenKeyW foi chamada com lpSubKey: %ls\n", wslpSubKey.c_str());
        std::cout << "[Contradef] Valor de retorno de RegOpenKeyW: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegOpenKeyW::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_Close(rtn);
}