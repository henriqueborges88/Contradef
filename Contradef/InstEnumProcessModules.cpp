#include "InstEnumProcessModules.h"

std::map<ADDRINT, InstEnumProcessModulesArgs> InstEnumProcessModules::argsMap;

VOID InstEnumProcessModules::CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded) {
    InstEnumProcessModulesArgs args;
    args.hProcess = hProcess;
    args.lphModule = lphModule;
    args.cb = cb;
    args.lpcbNeeded = lpcbNeeded;

    argsMap[instrPtr] = args;

    std::cout << "EnumProcessModules será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstEnumProcessModules::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de EnumProcessModules: " << *retValAddr << std::endl;
}

VOID InstEnumProcessModules::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);

    RTN_Close(rtn);

}
