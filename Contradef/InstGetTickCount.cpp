#include "InstGetTickCount.h"

/*
A função GetTickCount não possui argumentos de entrada. Ela retorna o número de milissegundos desde que o sistema foi iniciado. Portanto, a estrutura InstGetTickCountArgs é mantida vazia.
*/

VOID InstGetTickCount::CallbackBefore(ADDRINT instrPtr) {
    std::cout << "GetTickCount será chamada." << std::endl;
}

VOID InstGetTickCount::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr) {
    std::cout << "Valor de retorno de GetTickCount: " << *retValAddr << std::endl;
}

VOID InstGetTickCount::InstrumentFunction(RTN rtn) {
    PROTO protoGetTickCount = PROTO_Allocate(PIN_PARG(DWORD), CALLINGSTD_STDCALL,
        "GetTickCount",
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoGetTickCount,
        IARG_INST_PTR,
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoGetTickCount,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_END);

    PROTO_Free(protoGetTickCount);
}
