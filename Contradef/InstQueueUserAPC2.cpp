#include "InstQueueUserAPC2.h"

CallContextMap InstQueueUserAPC2::callContextMap;
UINT32 InstQueueUserAPC2::imgCallId = 0;
CallSequenceNumber InstQueueUserAPC2::fcnCallId = 0;
Notifier* InstQueueUserAPC2::globalNotifierPtr;

VOID InstQueueUserAPC2::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT pfnAPC, ADDRINT hThread, ADDRINT dwData) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    QueueUserAPC2Args* args = new QueueUserAPC2Args;
    args->pfnAPC = pfnAPC;
    args->hThread = hThread;
    args->dwData = dwData;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] QueueUserAPC2..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        pfnAPC: 0x" << std::hex << pfnAPC << std::dec << std::endl;
    stringStream << "        hThread: 0x" << std::hex << hThread << std::dec << std::endl;
    stringStream << "        dwData: " << dwData << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call QueueUserAPC2" << std::endl;

}

VOID InstQueueUserAPC2::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT pfnAPC, ADDRINT hThread, ADDRINT dwData) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from QueueUserAPC2: " << result << std::endl;

        if (result != 0) {
            stringStream << "    APC successfully queued." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to enqueue APC. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from QueueUserAPC2 completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const QueueUserAPC2Args* args = reinterpret_cast<QueueUserAPC2Args*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstQueueUserAPC2::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "QueueUserAPC2") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura de QueueUserAPC2 (assumida):
        // BOOL QueueUserAPC2(
        //   PAPCFUNC pfnAPC,
        //   HANDLE hThread,
        //   ULONG_PTR dwData
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // pfnAPC
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hThread
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwData
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (BOOL)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // pfnAPC
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hThread
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwData
            IARG_END);

        RTN_Close(rtn);
    }
}
