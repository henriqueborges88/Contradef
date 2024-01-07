#include "InstGetModuleHandleW.h"

std::map<ADDRINT, InstGetModuleHandleWArgs> InstGetModuleHandleW::argsMap;

VOID InstGetModuleHandleW::CallbackBefore(ADDRINT instrPtr, ADDRINT lpModuleName) {
    InstGetModuleHandleWArgs args;
    args.lpModuleName = lpModuleName;

    argsMap[instrPtr] = args;

    std::wcout << L"[Contradef] GetModuleHandleW será chamada com o seguinte parâmetro:" << std::endl;
    std::wcout << L"[Contradef] lpModuleName (Endereço): " << lpModuleName << std::endl;
}

VOID InstGetModuleHandleW::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpModuleName) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const InstGetModuleHandleWArgs& args = it->second;
        // Utilização dos valores armazenados
        // ...
    }

    std::wcout << L"[Contradef] Valor de retorno de GetModuleHandleW: " << *retValAddr << std::endl;
}

VOID InstGetModuleHandleW::InstrumentFunction(RTN rtn) {
    PROTO protoGetModuleHandleW = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "GetModuleHandleW",
        PIN_PARG(ADDRINT),   // lpModuleName
        PIN_PARG_END());

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoGetModuleHandleW,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoGetModuleHandleW,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
        IARG_END);

    RTN_Close(rtn);

    PROTO_Free(protoGetModuleHandleW);
}
