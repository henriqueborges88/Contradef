#include "InstIsDebuggerPresent.h"

/*
A função IsDebuggerPresent não possui argumentos, portanto, o método CallbackBefore apenas registra a chamada da função.
*/
VOID InstIsDebuggerPresent::CallbackBefore(ADDRINT instrPtr) {
    std::cout << "[Contradef] IsDebuggerPresent será chamada." << std::endl;
}

VOID InstIsDebuggerPresent::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr) {
    std::cout << "[Contradef] Valor de retorno de IsDebuggerPresent: " << *retValAddr << std::endl;
}

VOID InstIsDebuggerPresent::InstrumentFunction(RTN rtn) {
    PROTO protoIsDebuggerPresent = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "IsDebuggerPresent",
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoIsDebuggerPresent,
        IARG_INST_PTR,
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoIsDebuggerPresent,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_END);

    PROTO_Free(protoIsDebuggerPresent);
}
