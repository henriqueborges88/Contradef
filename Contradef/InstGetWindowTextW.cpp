#include "InstGetWindowTextW.h"

CallContextMap InstGetWindowTextW::callContextMap;
UINT32 InstGetWindowTextW::imgCallId = 0;
CallSequenceNumber InstGetWindowTextW::fcnCallId = 0;
Notifier* InstGetWindowTextW::globalNotifierPtr;


VOID InstGetWindowTextW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    GetWindowTextWArgs* args = new GetWindowTextWArgs;
    args->hWnd = hWnd;
    args->lpString = lpString;
    args->nMaxCount = nMaxCount;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    std::stringstream& stringStream = callContext->stringStream;
    std::wstring wideStringlpString = ConvertAddrToWideString(lpString);
    
    stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    stringStream << "    Module name: " << IMG_Name(img) << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hWnd: " << hWnd << std::endl;
    stringStream << "        lpString: " << WStringToString(wideStringlpString) << std::endl;
    stringStream << "        nMaxCount: " << nMaxCount << std::endl;


    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstGetWindowTextW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetWindowTextWArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetWindowTextW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetWindowTextW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
            IARG_END);

        RTN_Close(rtn);
    }

}
