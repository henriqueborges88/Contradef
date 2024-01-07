#include "InstGetModuleFileNameA.h"

std::map<ADDRINT, InstGetModuleFileNameAArgs> InstGetModuleFileNameA::argsMap;

VOID InstGetModuleFileNameA::CallbackBefore(ADDRINT instrPtr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameAArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    argsMap[instrPtr] = args;

    std::cout << "GetModuleFileNameA será chamada com os seguintes parâmetros: " << std::endl;
    // Impressão dos parâmetros
}

VOID InstGetModuleFileNameA::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de GetModuleFileNameA: " << *retValAddr << std::endl;
}

VOID InstGetModuleFileNameA::InstrumentFunction(RTN rtn) {
    PROTO protoGetModuleFileNameA = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "GetModuleFileNameA",
        PIN_PARG(ADDRINT), // hModule
        PIN_PARG(ADDRINT), // lpFilename
        PIN_PARG(ADDRINT), // nSize
        PIN_PARG_END());
    
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoGetModuleFileNameA,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoGetModuleFileNameA,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_END);
    
    RTN_Close(rtn);

    PROTO_Free(protoGetModuleFileNameA);
}
