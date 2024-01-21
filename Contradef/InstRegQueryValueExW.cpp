#include "InstRegQueryValueExW.h"

std::map<CallContextKey, RegQueryValueExWArgs> InstRegQueryValueExW::argsMap;
ADDRINT InstRegQueryValueExW::nextCallId = 0;

VOID InstRegQueryValueExW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    RegQueryValueExWArgs args;
    args.hKey = hKey;
    args.lpValueName = lpValueName;
    args.lpReserved = lpReserved;
    args.lpType = lpType;
    args.lpData = lpData;
    args.lpcbData = lpcbData;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegQueryValueExW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const RegQueryValueExWArgs& args = it->second;
        if (args.lpValueName != 0) {
            std::wstring wslpValueName = ConvertAddrToWideString(args.lpValueName);
            wprintf(L"[Contradef] RegQueryValueExW será chamada com lpValueName: %ls\n", wslpValueName.c_str());
        }
        std::cout << "[Contradef] Valor de retorno de RegQueryValueExW: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegQueryValueExW::InstrumentFunction(RTN rtn) {
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