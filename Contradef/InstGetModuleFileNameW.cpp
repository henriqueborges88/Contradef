#include "InstGetModuleFileNameW.h"

std::map<CallContextKey, InstGetModuleFileNameWArgs> InstGetModuleFileNameW::argsMap;
ADDRINT InstGetModuleFileNameW::nextCallId = 0;
std::ofstream* InstGetModuleFileNameW::outStream;

VOID InstGetModuleFileNameW::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameWArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleFileNameW::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstGetModuleFileNameWArgs& args = it->second;
        if (args.lpFilename != 0) {
            std::wstring wslpFilename = ConvertAddrToWideString(args.lpFilename);
            *outStream << std::endl << "[+] GetModuleFileNameW..." << std::endl;
            *outStream << "    lpFilename (parámetro): " << WStringToString(wslpFilename) << std::endl;
            *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
            *outStream << "[*] Concluído" << std::endl << std::endl;
        }
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstGetModuleFileNameW::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
        IARG_END);

    RTN_Close(rtn);

}