#include "InstGetModuleFileNameExW.h"

std::map<CallContextKey, InstGetModuleFileNameExWArgs> InstGetModuleFileNameExW::argsMap;
ADDRINT InstGetModuleFileNameExW::nextCallId = 0;

VOID InstGetModuleFileNameExW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameExWArgs args;
    args.hProcess = hProcess;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleFileNameExW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetModuleFileNameExWArgs& args = it->second;
        std::wstring wslpFilename = ConvertAddrToWideString(args.lpFilename);
        wprintf(L"[Contradef] GetModuleFileNameExW será chamada com lpFilename: %ls\n", wslpFilename.c_str());

        std::cout << "[Contradef] GetModuleFileNameExW completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetModuleFileNameExW::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
        IARG_END);

    RTN_Close(rtn);
}
