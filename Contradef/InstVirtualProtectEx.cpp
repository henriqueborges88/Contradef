#include "InstVirtualProtectEx.h"

std::map<CallContextKey, InstVirtualProtectExArgs> InstVirtualProtectEx::argsMap;
ADDRINT InstVirtualProtectEx::nextCallId = 0;
std::ofstream* InstVirtualProtectEx::outStream;

VOID InstVirtualProtectEx::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect) {
    InstVirtualProtectExArgs args;
    args.hProcess = hProcess;
    args.lpAddress = lpAddress;
    args.dwSize = dwSize;
    args.flNewProtect = flNewProtect;
    args.lpflOldProtect = lpflOldProtect;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstVirtualProtectEx::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstVirtualProtectExArgs& args = it->second;
        *outStream << std::endl << "[+] VirtualProtectEx..." << std::endl;
        *outStream << "    hProcess (parâmetro): " << args.hProcess << std::endl;
        *outStream << "    lpAddress (parâmetro): " << args.lpAddress << std::endl;
        *outStream << "    dwSize (parâmetro): " << args.dwSize << std::endl;
        *outStream << "    flNewProtect (parâmetro): " << args.flNewProtect << std::endl;
        *outStream << "    lpflOldProtect (parâmetro): " << args.lpflOldProtect << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstVirtualProtectEx::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwSize
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // flNewProtect
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpflOldProtect
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwSize
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // flNewProtect
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpflOldProtect
        IARG_END);

    RTN_Close(rtn);
}
