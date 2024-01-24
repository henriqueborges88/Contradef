#include "InstEnumProcessModules.h"

std::map<CallContextKey, InstEnumProcessModulesArgs> InstEnumProcessModules::argsMap;
ADDRINT InstEnumProcessModules::nextCallId = 0;
std::ofstream* InstEnumProcessModules::outStream;


VOID InstEnumProcessModules::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded) {
    InstEnumProcessModulesArgs args;
    args.hProcess = hProcess;
    args.lphModule = lphModule;
    args.cb = cb;
    args.lpcbNeeded = lpcbNeeded;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstEnumProcessModules::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        //Enumera os handles dos modulos do processo, pode estar seguido de GetModuleFileName
        *outStream << std::endl << "[+] EnumProcessModules..." << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstEnumProcessModules::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lphModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // cb
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbNeeded
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lphModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // cb
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbNeeded
        IARG_END);

    RTN_Close(rtn);

}