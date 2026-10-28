#include "InstFindWindowW.h"

CallContextMap InstFindWindowW::callContextMap;
UINT32 InstFindWindowW::imgCallId = 0;
CallSequenceNumber InstFindWindowW::fcnCallId = 0;
Notifier* InstFindWindowW::globalNotifierPtr;


VOID InstFindWindowW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpClassName, ADDRINT lpWindowName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstFindWindowWArgs* args = new InstFindWindowWArgs;
    args->lpClassName = lpClassName;
    args->lpWindowName = lpWindowName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    std::stringstream& stringStream = callContext->stringStream;
    std::wstring className = ConvertAddrToWideString(lpClassName);
    std::wstring windowName = ConvertAddrToWideString(lpWindowName);
    std::string ansiClassName(className.begin(), className.end());
    std::string ansiWindowName(windowName.begin(), windowName.end());

    
    stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    stringStream << "    Module name: " << IMG_Name(img) << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpClassName: " << ansiClassName << std::endl;
    stringStream << "        lpWindowName: " << ansiWindowName << std::endl;
    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstFindWindowW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT lpClassName, ADDRINT lpWindowName) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        // Registrar Parámetros
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<InstFindWindowWArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstFindWindowW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "FindWindowW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpWindowName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpWindowName
            IARG_END);

        RTN_Close(rtn);
    }
}