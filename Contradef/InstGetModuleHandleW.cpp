#include "InstGetModuleHandleW.h"

std::map<CallContextKey, InstGetModuleHandleWArgs> InstGetModuleHandleW::argsMap;
ADDRINT InstGetModuleHandleW::nextCallId = 0;

VOID InstGetModuleHandleW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpModuleName) {
    InstGetModuleHandleWArgs args;
    args.lpModuleName = lpModuleName;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleHandleW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpModuleName) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetModuleHandleWArgs& args = it->second;
        if (args.lpModuleName != 0) {
            std::wstring wslpModuleName = ConvertAddrToWideString(args.lpModuleName);
            wprintf(L"[Contradef] GetModuleHandleW foi chamada com lpModuleName: %ls\n", wslpModuleName.c_str());
            std::cout << "[Contradef] Valor de retorno de GetModuleHandleW: " << *retValAddr << std::endl;
        }
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetModuleHandleW::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
        IARG_END);

    RTN_Close(rtn);
}
