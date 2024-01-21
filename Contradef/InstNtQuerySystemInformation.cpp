#include "InstNtQuerySystemInformation.h"

std::map<CallContextKey, InstNtQuerySystemInformationArgs> InstNtQuerySystemInformation::argsMap;
ADDRINT InstNtQuerySystemInformation::nextCallId = 0;

VOID InstNtQuerySystemInformation::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength) {
    InstNtQuerySystemInformationArgs args;
    args.SystemInformationClass = SystemInformationClass;
    args.SystemInformation = SystemInformation;
    args.SystemInformationLength = SystemInformationLength;
    args.ReturnLength = ReturnLength;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstNtQuerySystemInformation::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        std::cout << "[Contradef] Valor de retorno de NtQuerySystemInformation: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstNtQuerySystemInformation::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // SystemInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // SystemInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // SystemInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ReturnLength
        IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // SystemInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // SystemInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // SystemInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ReturnLength
        IARG_END);

    RTN_Close(rtn);

}