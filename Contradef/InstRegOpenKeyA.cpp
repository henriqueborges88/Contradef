#include "InstRegOpenKeyA.h"

std::map<CallContextKey, InstRegOpenKeyAArgs> InstRegOpenKeyA::argsMap;
ADDRINT InstRegOpenKeyA::nextCallId = 0;
std::ofstream* InstRegOpenKeyA::outStream;

VOID InstRegOpenKeyA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    InstRegOpenKeyAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstRegOpenKeyA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstRegOpenKeyAArgs& args = it->second;
        std::string ansiStringLpSubKey = ConvertAddrToAnsiString(args.lpSubKey);
        if (ansiStringLpSubKey == "HARDWARE\ACPI\DSDT\VBOX__") {
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //*retValAddr = 0;
        }
        // TODO: criar includelist e excludelist
        // mostrar todos menos exclude list caso esteja definido
        // excluir todos menos includelist caso esteja definido
        *outStream << std::endl << "[+] RegOpenKeyA..." << std::endl;
        *outStream << "    lpSubKey (parámetro): " << ansiStringLpSubKey << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstRegOpenKeyA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_Close(rtn);
}

