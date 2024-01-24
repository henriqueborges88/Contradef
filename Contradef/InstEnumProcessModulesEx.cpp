#include "InstEnumProcessModulesEx.h"

std::map<CallContextKey, InstEnumProcessModulesExArgs> InstEnumProcessModulesEx::argsMap;
ADDRINT InstEnumProcessModulesEx::nextCallId = 0;
std::ofstream* InstEnumProcessModulesEx::outStream;

VOID InstEnumProcessModulesEx::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag) {
    InstEnumProcessModulesExArgs args;
    args.hProcess = hProcess;
    args.lphModule = lphModule;
    args.cb = cb;
    args.lpcbNeeded = lpcbNeeded;
    args.dwFilterFlag = dwFilterFlag;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstEnumProcessModulesEx::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        //Enumera os handles dos modulos do processo, pode estar seguido de GetModuleFileName
        *outStream << std::endl << "[+] EnumProcessModulesEx..." << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstEnumProcessModulesEx::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lphModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // cb
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbNeeded
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // dwFilterFlag
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lphModule
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // cb
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcbNeeded
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // dwFilterFlag
        IARG_END);

    RTN_Close(rtn);

}