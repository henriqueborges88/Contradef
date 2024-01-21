#include "InstRegQueryValueExA.h"

std::map<CallContextKey, InstRegQueryValueExAArgs> InstRegQueryValueExA::argsMap;
ADDRINT InstRegQueryValueExA::nextCallId = 0;

VOID InstRegQueryValueExA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    InstRegQueryValueExAArgs args;
    args.hKey = hKey;
    args.lpValueName = lpValueName;
    args.lpReserved = lpReserved;
    args.lpType = lpType;
    args.lpData = lpData;
    args.lpcbData = lpcbData;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegQueryValueExA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegQueryValueExAArgs& args = it->second;
        std::string ansiStringlpValueName = ConvertAddrToAnsiString(args.lpValueName);
        std::cout << "[Contradef] RegQueryValueExA invocada com lpValueName: " << ansiStringlpValueName << std::endl;
        std::cout << "[Contradef] RegQueryValueExA completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegQueryValueExA::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
        IARG_END);

    RTN_Close(rtn);
}