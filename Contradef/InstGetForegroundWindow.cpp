#include "InstGetForegroundWindow.h"

/*
A função GetForegroundWindow não possui argumentos. Ela simplesmente retorna o identificador da janela atualmente em primeiro plano. Por isso, o método CallbackBefore apenas registra a chamada da função, e o CallbackAfter registra o valor de retorno.
*/

VOID InstGetForegroundWindow::CallbackBefore(ADDRINT instrPtr) {
    std::cout << "[Contradef] GetForegroundWindow será chamada." << std::endl;
}

VOID InstGetForegroundWindow::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr) {
    // Esta função não esta sendo chamada, verificar o motivo
    std::cout << "[Contradef] GetForegroundWindow foi chamado, o valor de retorno eh: " << *retValAddr << std::endl;
}

VOID InstGetForegroundWindow::InstrumentFunction(RTN rtn) {

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
