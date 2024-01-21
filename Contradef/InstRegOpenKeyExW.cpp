#include "InstRegOpenKeyExW.h"

std::map<CallContextKey, RegOpenKeyExWArgs> InstRegOpenKeyExW::argsMap;
ADDRINT InstRegOpenKeyExW::nextCallId = 0;

VOID InstRegOpenKeyExW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    RegOpenKeyExWArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.ulOptions = ulOptions;
    args.samDesired = samDesired;
    args.phkResult = phkResult;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegOpenKeyExW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const RegOpenKeyExWArgs& args = it->second;
        if (args.lpSubKey != 0) {
            std::wstring wslpSubKey = ConvertAddrToWideString(args.lpSubKey);
            wprintf(L"[Contradef] RegOpenKeyExW foi chamada com lpSubKey: %ls\n", wslpSubKey.c_str());
        }
        std::cout << "[Contradef] Valor de retorno de RegOpenKeyExW: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegOpenKeyExW::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
        IARG_END);

    RTN_Close(rtn);

}