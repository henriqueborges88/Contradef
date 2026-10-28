#include "InstGetCurrentProcess.h"

CallContextMap InstGetCurrentProcess::callContextMap;
UINT32 InstGetCurrentProcess::imgCallId = 0;
CallSequenceNumber InstGetCurrentProcess::fcnCallId = 0;
Notifier* InstGetCurrentProcess::globalNotifierPtr;

VOID InstGetCurrentProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, nullptr);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando a chamada
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetCurrentProcess..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of getting the handle of the current process" << std::endl;

}

VOID InstGetCurrentProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT retVal, ADDRINT* returnAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Current process handle: " << std::hex << retVal << std::dec << std::endl;
        stringStream << "  [-] Obtaining the current process handle completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetCurrentProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetCurrentProcess") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,          // Endereço da função chamante
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,  // Valor de retorno da função
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_END);

        RTN_Close(rtn);
    }
}
