#include "InstIsDebuggerPresent.h"

ADDRINT InstIsDebuggerPresent::nextCallId = 0;

VOID InstIsDebuggerPresent::CallbackBefore(THREADID tid, ADDRINT callId) {
    //std::cout << "[Contradef] IsDebuggerPresent será chamada." << std::endl;
}

VOID InstIsDebuggerPresent::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr) {
    std::cout << "[Contradef] Valor de retorno de IsDebuggerPresent: " << *retValAddr << std::endl;
}

VOID InstIsDebuggerPresent::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_END);

    RTN_Close(rtn);

}