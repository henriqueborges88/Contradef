#include "InstRegQueryValueA.h"

std::map<CallContextKey, InstRegQueryValueAArgs> InstRegQueryValueA::argsMap;
ADDRINT InstRegQueryValueA::nextCallId = 0;

VOID InstRegQueryValueA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue) {
    InstRegQueryValueAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.lpValue = lpValue;
    args.lpcbValue = lpcbValue;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegQueryValueA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegQueryValueAArgs& args = it->second;
        std::string ansiStringlpSubKey = ConvertAddrToAnsiString(args.lpSubKey);
        std::string ansiStringlpValue = ConvertAddrToAnsiString(args.lpValue);
        std::string ansiStringlpcbValue = ConvertAddrToAnsiString(args.lpcbValue);

        std::cout << "[Contradef] RegQueryValueA invocada com lpSubKey: " << ansiStringlpSubKey << std::endl;
        std::cout << "[Contradef] RegQueryValueA invocada com lpValue: " << ansiStringlpValue << std::endl;
        std::cout << "[Contradef] RegQueryValueA invocada com lpcbValue: " << ansiStringlpcbValue << std::endl;
        std::cout << "[Contradef] RegQueryValueA completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegQueryValueA::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpValue
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbValue
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpValue
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbValue
        IARG_END);

    RTN_Close(rtn);
}
