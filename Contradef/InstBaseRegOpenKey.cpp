#include "InstBaseRegOpenKey.h"

std::map<ADDRINT, InstBaseRegOpenKeyArgs> InstBaseRegOpenKey::argsMap;

VOID InstBaseRegOpenKey::CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    InstBaseRegOpenKeyArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    argsMap[instrPtr] = args;

    std::cout << "BaseRegOpenKey será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstBaseRegOpenKey::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de BaseRegOpenKey: " << *retValAddr << std::endl;
}

VOID InstBaseRegOpenKey::InstrumentFunction(RTN rtn) {
    if (RTN_Name(rtn) == "BaseRegOpenKey") {
        
        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_INST_PTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_INST_PTR,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_END);

        RTN_Close(rtn);
    }
}
