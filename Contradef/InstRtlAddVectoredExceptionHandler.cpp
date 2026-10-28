#include "InstRtlAddVectoredExceptionHandler.h"

CallContextMap InstRtlAddVectoredExceptionHandler::callContextMap;
UINT32 InstRtlAddVectoredExceptionHandler::imgCallId = 0;
CallSequenceNumber InstRtlAddVectoredExceptionHandler::fcnCallId = 0;
Notifier* InstRtlAddVectoredExceptionHandler::globalNotifierPtr;

VOID InstRtlAddVectoredExceptionHandler::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT First, ADDRINT Handler) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RtlAddVectoredExceptionHandlerArgs* args = new RtlAddVectoredExceptionHandlerArgs;
    args->First = First;
    args->Handler = Handler;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] RtlAddVectoredExceptionHandler..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        First: " << First << std::endl; // Exibe o valor numericamente
    stringStream << "        Handler: 0x" << std::hex << Handler << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call RtlAddVectoredExceptionHandler" << std::endl;

}

VOID InstRtlAddVectoredExceptionHandler::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT First, ADDRINT Handler) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        PVOID result = reinterpret_cast<PVOID>(retValue);
        stringStream << "    Returned from RtlAddVectoredExceptionHandler: 0x" << std::hex << retValue << std::dec << std::endl;

        if (result != NULL) {
            stringStream << "    Exception handler added successfully." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to add exception handler. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from RtlAddVectoredExceptionHandler completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const RtlAddVectoredExceptionHandlerArgs* args = reinterpret_cast<RtlAddVectoredExceptionHandlerArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstRtlAddVectoredExceptionHandler::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RtlAddVectoredExceptionHandler") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // PVOID RtlAddVectoredExceptionHandler(
        //   ULONG First,
        //   PVECTORED_EXCEPTION_HANDLER Handler
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // First
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Handler
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (PVOID)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // First
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Handler
            IARG_END);

        RTN_Close(rtn);
    }
}
