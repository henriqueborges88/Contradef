#include "InstWmi.h"

CallContextMap InstWmi::callContextMap;
UINT32 InstWmi::imgCallId = 0;
CallSequenceNumber InstWmi::fcnCallId = 0;
Notifier* InstWmi::globalNotifierPtr;


VOID InstWmi::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstWmiArgs* args = new InstWmiArgs;
    args->arg1 = arg1;
    args->arg2 = arg2;
    args->arg3 = arg3;
    args->arg4 = arg4;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstWmi::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {

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
        const InstWmiArgs* args = reinterpret_cast<InstWmiArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        arg1: " << args->arg1 << std::endl;
        stringStream << "        arg2: " << args->arg2 << std::endl;
        stringStream << "        arg3: " << args->arg3 << std::endl;
        stringStream << "        arg4: " << args->arg4 << std::endl;
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

VOID InstWmi::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CoCreateInstance" || rtnName == "ConnectServer" || rtnName == "ExecQuery") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // arg1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // arg2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // arg3
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // arg4
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // arg1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // arg2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // arg3
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // arg4
            IARG_END);

        RTN_Close(rtn);
    }
}
