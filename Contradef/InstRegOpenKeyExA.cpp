#include "InstRegOpenKeyExA.h"

std::map<ADDRINT, InstRegOpenKeyExAArgs> InstRegOpenKeyExA::argsMap;

VOID InstRegOpenKeyExA::CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    InstRegOpenKeyExAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.ulOptions = ulOptions;
    args.samDesired = samDesired;
    args.phkResult = phkResult;

    argsMap[instrPtr] = args;

    std::cout << "RegOpenKeyExA será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstRegOpenKeyExA::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de RegOpenKeyExA: " << *retValAddr << std::endl;
}

VOID InstRegOpenKeyExA::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4,
        IARG_END);

    RTN_Close(rtn);

}
