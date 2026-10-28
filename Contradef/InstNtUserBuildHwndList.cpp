#include "InstNtUserBuildHwndList.h"

CallContextMap InstNtUserBuildHwndList::callContextMap;
UINT32 InstNtUserBuildHwndList::imgCallId = 0;
CallSequenceNumber InstNtUserBuildHwndList::fcnCallId = 0;
Notifier* InstNtUserBuildHwndList::globalNotifierPtr;


VOID InstNtUserBuildHwndList::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hDesktop, ADDRINT hwndParent, ADDRINT hwndChild, ADDRINT bEnumChildren, ADDRINT pWnds, ADDRINT nWnds) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtUserBuildHwndListArgs* args = new InstNtUserBuildHwndListArgs;
    args->hDesktop = hDesktop;
    args->hwndParent = hwndParent;
    args->hwndChild = hwndChild;
    args->bEnumChildren = bEnumChildren;
    args->pWnds = pWnds;
    args->nWnds = nWnds;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstNtUserBuildHwndList::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT hDesktop, ADDRINT hwndParent, ADDRINT hwndChild, ADDRINT bEnumChildren, ADDRINT pWnds, ADDRINT nWnds) {

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
        const InstNtUserBuildHwndListArgs* args = reinterpret_cast<InstNtUserBuildHwndListArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hDesktop: " << args->hDesktop << std::endl;
        stringStream << "        hwndParent: " << args->hwndParent << std::endl;
        stringStream << "        hwndChild: " << args->hwndChild << std::endl;
        stringStream << "        bEnumChildren: " << args->bEnumChildren << std::endl;
        stringStream << "        pWnds: " << args->pWnds << std::endl;
        stringStream << "        nWnds: " << args->nWnds << std::endl;
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

VOID InstNtUserBuildHwndList::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtUserBuildHwndList" || rtnName == "ZwUserBuildHwndList") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hDesktop
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hwndChild
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // bEnumChildren
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // pWnds
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nWnds
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hDesktop
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hwndChild
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // bEnumChildren
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // pWnds
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nWnds
            IARG_END);

        RTN_Close(rtn);
    }
}
