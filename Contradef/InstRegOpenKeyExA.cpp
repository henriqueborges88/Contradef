#include "InstRegOpenKeyExA.h"

std::map<CallContextKey, InstRegOpenKeyExAArgs> InstRegOpenKeyExA::argsMap;
ADDRINT InstRegOpenKeyExA::nextCallId = 0;
std::ofstream* InstRegOpenKeyExA::outStream;

VOID InstRegOpenKeyExA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    InstRegOpenKeyExAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.ulOptions = ulOptions;
    args.samDesired = samDesired;
    args.phkResult = phkResult;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegOpenKeyExA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegOpenKeyExAArgs& args = it->second;
        std::string ansiStringLpSubKey = ConvertAddrToAnsiString(args.lpSubKey);
        *outStream << std::endl << "[+] RegOpenKeyExA..." << std::endl;
        *outStream << "    lpSubKey (parámetro): " << ansiStringLpSubKey << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegOpenKeyExA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
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