#include "InstNtQuerySystemInformationEx.h"

CallContextMap InstNtQuerySystemInformationEx::callContextMap;
UINT32 InstNtQuerySystemInformationEx::imgCallId = 0;
CallSequenceNumber InstNtQuerySystemInformationEx::fcnCallId = 0;
Notifier* InstNtQuerySystemInformationEx::globalNotifierPtr;

VOID InstNtQuerySystemInformationEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT SystemInformationClass, ADDRINT InputBuffer, ULONG InputBufferLength,
    ADDRINT SystemInformation, ULONG SystemInformationLength, ADDRINT ReturnLength) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    NtQuerySystemInformationExArgs* args = new NtQuerySystemInformationExArgs;
    args->SystemInformationClass = SystemInformationClass;
    args->InputBuffer = InputBuffer;
    args->InputBufferLength = InputBufferLength;
    args->SystemInformation = SystemInformation;
    args->SystemInformationLength = SystemInformationLength;
    args->ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;

    stringStream << std::endl << "[+] NtQuerySystemInformationEx..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        SystemInformationClass: " << SystemInformationClass << std::endl;
    stringStream << "        InputBuffer: " << std::hex << InputBuffer << std::dec << std::endl;
    stringStream << "        InputBufferLength: " << InputBufferLength << std::endl;
    stringStream << "        SystemInformation: " << std::hex << SystemInformation << std::dec << std::endl;
    stringStream << "        SystemInformationLength: " << SystemInformationLength << std::endl;
    stringStream << "        ReturnLength: " << std::hex << ReturnLength << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call NtQuerySystemInformationEx" << std::endl;

}

VOID InstNtQuerySystemInformationEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Return Value: 0x" << std::hex << retVal << std::dec << std::endl;
        stringStream << "  [-] Call from NtQuerySystemInformationEx completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const NtQuerySystemInformationExArgs* args = reinterpret_cast<NtQuerySystemInformationExArgs*>(callContext->functionArgs);
        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstNtQuerySystemInformationEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtQuerySystemInformationEx" || rtnName == "ZwQuerySystemInformationEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                        // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // SystemInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // InputBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // InputBufferLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // SystemInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,      // SystemInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,      // ReturnLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno
            IARG_END);

        RTN_Close(rtn);
    }
}
