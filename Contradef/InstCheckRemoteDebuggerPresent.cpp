#include "InstCheckRemoteDebuggerPresent.h"

std::map<CallContextKey, InstCheckRemoteDebuggerPresentArgs> InstCheckRemoteDebuggerPresent::argsMap;
ADDRINT InstCheckRemoteDebuggerPresent::nextCallId = 0;

VOID InstCheckRemoteDebuggerPresent::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {
    InstCheckRemoteDebuggerPresentArgs args;
    args.hProcess = hProcess;
    args.pbDebuggerPresent = pbDebuggerPresent;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;

}

VOID InstCheckRemoteDebuggerPresent::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstCheckRemoteDebuggerPresentArgs& args = it->second;

        u_short* NtGlobalFlagPtr = reinterpret_cast<u_short*>(args.pbDebuggerPresent);
        std::cout << "[Contradef] CheckRemoteDebuggerPresent foi chamada, o retorno de pbDebuggerPresent eh: " << *NtGlobalFlagPtr << std::endl;
        std::cout << "[Contradef] Valor de retorno da função CheckRemoteDebuggerPresent: " << *retValAddr << std::endl;
    }
}

VOID InstCheckRemoteDebuggerPresent::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
        IARG_END);
    RTN_Close(rtn);

}