#include "InstGetModuleFileNameA.h"

std::map<ADDRINT, InstGetModuleFileNameAArgs> InstGetModuleFileNameA::argsMap;

VOID InstGetModuleFileNameA::CallbackBefore(ADDRINT instrPtr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameAArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    argsMap[instrPtr] = args;

}

VOID InstGetModuleFileNameA::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const InstGetModuleFileNameAArgs& args = it->second;
        // Operações com args, se necessário   
        // Impressão dos parâmetros
        const char* lpFilenameStr = reinterpret_cast<const char*>(args.lpFilename);
        std::cout << "[Contradef] GetModuleFileNameA invocada com lpFilename: " << lpFilenameStr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    //std::cout << "Valor de retorno de GetModuleFileNameA: " << *retValAddr << std::endl;

}

VOID InstGetModuleFileNameA::InstrumentFunction(RTN rtn) {
    
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
