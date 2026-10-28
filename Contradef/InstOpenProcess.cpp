#include "InstOpenProcess.h"

CallContextMap InstOpenProcess::callContextMap;
UINT32 InstOpenProcess::imgCallId = 0;
CallSequenceNumber InstOpenProcess::fcnCallId = 0;
Notifier* InstOpenProcess::globalNotifierPtr;


VOID InstOpenProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT dwDesiredAccess, ADDRINT bInheritHandle, ADDRINT dwProcessId) {
    
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstOpenProcessArgs* args = new InstOpenProcessArgs;
    args->dwDesiredAccess = dwDesiredAccess;
    args->bInheritHandle = bInheritHandle;
    args->dwProcessId = dwProcessId;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstOpenProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT dwDesiredAccess, ADDRINT bInheritHandle, ADDRINT dwProcessId) {

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
        const InstOpenProcessArgs* args = reinterpret_cast<InstOpenProcessArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        dwDesiredAccess: " << args->dwDesiredAccess << std::endl;
        stringStream << "        bInheritHandle: " << args->bInheritHandle << std::endl;
        stringStream << "        dwProcessId: " << args->dwProcessId << std::endl;
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

VOID InstOpenProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "OpenProcess") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // bInheritHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwProcessId
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // bInheritHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwProcessId
            IARG_END);

        RTN_Close(rtn);
    }
}
