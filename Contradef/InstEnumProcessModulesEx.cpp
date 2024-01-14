#include "InstEnumProcessModulesEx.h"

std::map<ADDRINT, InstEnumProcessModulesExArgs> InstEnumProcessModulesEx::argsMap;

VOID InstEnumProcessModulesEx::CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag) {
    InstEnumProcessModulesExArgs args;
    args.hProcess = hProcess;
    args.lphModule = lphModule;
    args.cb = cb;
    args.lpcbNeeded = lpcbNeeded;
    args.dwFilterFlag = dwFilterFlag;

    argsMap[instrPtr] = args;

    std::cout << "EnumProcessModulesEx será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstEnumProcessModulesEx::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de EnumProcessModulesEx: " << *retValAddr << std::endl;
}

VOID InstEnumProcessModulesEx::InstrumentFunction(RTN rtn) {

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
