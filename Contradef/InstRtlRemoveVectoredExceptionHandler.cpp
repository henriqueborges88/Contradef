#include "InstRtlRemoveVectoredExceptionHandler.h"

std::map<CallContextKey, CallContext*> InstRtlRemoveVectoredExceptionHandler::callContextMap;
UINT32 InstRtlRemoveVectoredExceptionHandler::imgCallId = 0;
UINT32 InstRtlRemoveVectoredExceptionHandler::fcnCallId = 0;
Notifier* InstRtlRemoveVectoredExceptionHandler::globalNotifierPtr;

VOID InstRtlRemoveVectoredExceptionHandler::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT Handle) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RtlRemoveVectoredExceptionHandlerArgs* args = new RtlRemoveVectoredExceptionHandlerArgs;
    args->Handle = Handle;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] RtlRemoveVectoredExceptionHandler..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        Handle: 0x" << std::hex << Handle << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call RtlRemoveVectoredExceptionHandler" << std::endl;

}

VOID InstRtlRemoveVectoredExceptionHandler::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT Handle) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        ULONG result = static_cast<ULONG>(retValue);
        stringStream << "    Returned from RtlRemoveVectoredExceptionHandler: " << result << std::endl;

        if (result != 0) {
            stringStream << "    Exception handler successfully removed." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to remove exception handler. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from RtlRemoveVectoredExceptionHandler completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const RtlRemoveVectoredExceptionHandlerArgs* args = reinterpret_cast<RtlRemoveVectoredExceptionHandlerArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstRtlRemoveVectoredExceptionHandler::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RtlRemoveVectoredExceptionHandler") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // ULONG RtlRemoveVectoredExceptionHandler(PVOID Handle);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (ULONG)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
            IARG_END);

        RTN_Close(rtn);
    }
}
