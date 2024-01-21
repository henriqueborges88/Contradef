#include "InstRegQueryValueW.h"

std::map<CallContextKey, InstRegQueryValueWArgs> InstRegQueryValueW::argsMap;
ADDRINT InstRegQueryValueW::nextCallId = 0;

VOID InstRegQueryValueW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue) {
    InstRegQueryValueWArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.lpValue = lpValue;
    args.lpcbValue = lpcbValue;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegQueryValueW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT lpValue, ADDRINT lpcbValue) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegQueryValueWArgs& args = it->second;
        std::wstring wslpSubKey = ConvertAddrToWideString(args.lpSubKey);
        std::wstring wslpValue = ConvertAddrToWideString(args.lpValue);
        std::wstring wslpcbValue = ConvertAddrToWideString(args.lpcbValue);
        wprintf(L"[Contradef] RegQueryValueW foi chamada com lpSubKey: %ls\n", wslpSubKey.c_str());
        wprintf(L"[Contradef] RegQueryValueW foi chamada com lpValue: %ls\n", wslpValue.c_str());
        wprintf(L"[Contradef] RegQueryValueW foi chamada com lpcbValue: %ls\n", wslpcbValue.c_str());
        std::cout << "[Contradef] RegQueryValueW completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegQueryValueW::InstrumentFunction(RTN rtn) {
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
