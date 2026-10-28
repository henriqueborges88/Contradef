#include "InstNtUserFindWindowEx.h"

CallContextMap InstNtUserFindWindowEx::callContextMap;
UINT32 InstNtUserFindWindowEx::imgCallId = 0;
CallSequenceNumber InstNtUserFindWindowEx::fcnCallId = 0;
Notifier* InstNtUserFindWindowEx::globalNotifierPtr;


VOID InstNtUserFindWindowEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hwndParent, ADDRINT hwndChildAfter, ADDRINT lpszClass, ADDRINT lpszWindow) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtUserFindWindowExArgs* args = new InstNtUserFindWindowExArgs;
    args->hwndParent = hwndParent;
    args->hwndChildAfter = hwndChildAfter;
    args->lpszClass = lpszClass;
    args->lpszWindow = lpszWindow;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstNtUserFindWindowEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT hwndParent, ADDRINT hwndChildAfter, ADDRINT lpszClass, ADDRINT lpszWindow) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = pendingCallContext;
        // Registrar Parámetros
        const InstNtUserFindWindowExArgs* args = reinterpret_cast<InstNtUserFindWindowExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string className = args->lpszClass ? ConvertAddrToAnsiString(args->lpszClass) : "NULL";
        std::string windowName = args->lpszWindow ? ConvertAddrToAnsiString(args->lpszWindow) : "NULL";

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hwndParent: " << args->hwndParent << std::endl;
        stringStream << "        hwndChildAfter: " << args->hwndChildAfter << std::endl;
        stringStream << "        lpszClass: " << className << std::endl;
        stringStream << "        lpszWindow: " << windowName << std::endl;
        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstNtUserFindWindowEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtUserFindWindowEx" || rtnName == "ZwUserFindWindowEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndChildAfter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpszClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpszWindow
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndChildAfter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpszClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpszWindow
            IARG_END);

        RTN_Close(rtn);
    }
}
