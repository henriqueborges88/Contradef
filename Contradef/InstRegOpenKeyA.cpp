#include "InstRegOpenKeyA.h"

std::map<ADDRINT, InstRegOpenKeyAArgs> InstRegOpenKeyA::argsMap;

VOID InstRegOpenKeyA::CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    InstRegOpenKeyAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    argsMap[instrPtr] = args;

    std::wcout << L"RegOpenKeyA será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstRegOpenKeyA::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::wcout << L"Valor de retorno de RegOpenKeyA: " << *retValAddr << std::endl;
}

VOID InstRegOpenKeyA::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
        IARG_END);

    RTN_Close(rtn);

}
