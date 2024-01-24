#include "InstIsDebuggerPresent.h"

ADDRINT InstIsDebuggerPresent::nextCallId = 0;
std::ofstream* InstIsDebuggerPresent::outStream;

VOID InstIsDebuggerPresent::CallbackBefore(THREADID tid, ADDRINT callId) {
    //*outStream << "IsDebuggerPresent" << std::endl;
}

VOID InstIsDebuggerPresent::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr) {
    
    *outStream << std::endl << "[+] IsDebuggerPresent..." << std::endl;
    *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
    *outStream << "[*] Concluído" << std::endl << std::endl;

}

VOID InstIsDebuggerPresent::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

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