#include "InstNtSetDebugFilterState.h"

std::map<CallContextKey, CallContext*> InstNtSetDebugFilterState::callContextMap;
UINT32 InstNtSetDebugFilterState::imgCallId = 0;
UINT32 InstNtSetDebugFilterState::fcnCallId = 0;
Notifier* InstNtSetDebugFilterState::globalNotifierPtr;


VOID InstNtSetDebugFilterState::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ComponentId, ADDRINT Level, ADDRINT State) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtSetDebugFilterStateArgs* args = new InstNtSetDebugFilterStateArgs;
    args->ComponentId = ComponentId;
    args->Level = Level;
    args->State = State;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    }

VOID InstNtSetDebugFilterState::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT ComponentId, ADDRINT Level, ADDRINT State) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = it->second;
        // Registrar Parámetros
        const InstNtSetDebugFilterStateArgs* args = reinterpret_cast<InstNtSetDebugFilterStateArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        ComponentId: " << args->ComponentId << std::endl;
        stringStream << "        Level: " << args->Level << std::endl;
        stringStream << "        State: " << args->State << std::endl;
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

VOID InstNtSetDebugFilterState::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtSetDebugFilterState" || rtnName == "ZwSetDebugFilterState") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ComponentId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Level
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // State
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ComponentId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Level
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // State
            IARG_END);

        RTN_Close(rtn);
    }
}
