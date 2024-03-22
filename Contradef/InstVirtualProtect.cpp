#include "InstVirtualProtect.h"

std::map<CallContextKey, InstVirtualProtectArgs> InstVirtualProtect::argsMap;
ADDRINT InstVirtualProtect::nextCallId = 0;
std::ofstream* InstVirtualProtect::outStream;

VOID InstVirtualProtect::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect) {
    InstVirtualProtectArgs args;
    args.lpAddress = lpAddress;
    args.dwSize = dwSize;
    args.flNewProtect = flNewProtect;
    args.lpflOldProtect = lpflOldProtect;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstVirtualProtect::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstVirtualProtectArgs& args = it->second;
        *outStream << std::endl << "[+] VirtualProtect..." << std::endl;
        *outStream << "    lpAddress (parâmetro): " << args.lpAddress << std::endl;
        *outStream << "    dwSize (parâmetro): " << args.dwSize << std::endl;
        *outStream << "    flNewProtect (parâmetro): " << args.flNewProtect << std::endl;
        *outStream << "    lpflOldProtect (parâmetro): " << args.lpflOldProtect << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstVirtualProtect::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwSize
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // flNewProtect
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpflOldProtect
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpAddress
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwSize
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // flNewProtect
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpflOldProtect
        IARG_END);

    RTN_Close(rtn);
}
