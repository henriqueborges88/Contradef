#include "InstGetTickCount.h"

/*
A função GetTickCount não possui argumentos de entrada. Ela retorna o número de milissegundos desde que o sistema foi iniciado. Portanto, a estrutura InstGetTickCountArgs é mantida vazia.
*/

VOID InstGetTickCount::CallbackBefore(ADDRINT instrPtr) {
    //std::cout << "GetTickCount será chamada." << std::endl;
}

VOID InstGetTickCount::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr) {
    std::cout << "[Contradef] GetTickCount foi chamada, o valor de retorno eh: " << *retValAddr << std::endl;
}

VOID InstGetTickCount::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_END);

    RTN_Close(rtn);

}
