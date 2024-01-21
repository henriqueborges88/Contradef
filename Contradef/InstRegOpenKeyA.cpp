#include "InstRegOpenKeyA.h"

std::map<CallContextKey, InstRegOpenKeyAArgs> InstRegOpenKeyA::argsMap;
ADDRINT InstRegOpenKeyA::nextCallId = 0;

VOID InstRegOpenKeyA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    InstRegOpenKeyAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegOpenKeyA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegOpenKeyAArgs& args = it->second;
        std::string ansiStringLpSubKey = ConvertAddrToAnsiString(args.lpSubKey);
        std::cout << "[Contradef] RegOpenKeyA invocada com lpSubKey: " << ansiStringLpSubKey << std::endl;
        if (ansiStringLpSubKey == "HARDWARE\ACPI\DSDT\VBOX__") {
            //*retValAddr = 0;
        }
        std::cout << "[Contradef] Valor de retorno de RegOpenKeyA: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegOpenKeyA::InstrumentFunction(RTN rtn) {
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

