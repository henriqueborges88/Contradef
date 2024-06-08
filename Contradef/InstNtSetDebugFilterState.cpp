#include "InstNtSetDebugFilterState.h"

std::map<CallContextKey, CallContext*> InstNtSetDebugFilterState::callContextMap;
UINT32 InstNtSetDebugFilterState::imgCallId = 0;
UINT32 InstNtSetDebugFilterState::fcnCallId = 0;
Notifier* InstNtSetDebugFilterState::globalNotifierPtr;

VOID InstNtSetDebugFilterState::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtSetDebugFilterState::callContextMap, InstNtSetDebugFilterState::globalNotifierPtr);
}

VOID InstNtSetDebugFilterState::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtSetDebugFilterState::callContextMap, InstNtSetDebugFilterState::globalNotifierPtr);
}

VOID InstNtSetDebugFilterState::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtSetDebugFilterState::callContextMap, InstNtSetDebugFilterState::globalNotifierPtr);
}

VOID InstNtSetDebugFilterState::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ComponentId, ADDRINT Level, ADDRINT State) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtSetDebugFilterStateArgs args;
    args.ComponentId = ComponentId;
    args.Level = Level;
    args.State = State;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtSetDebugFilterState::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtSetDebugFilterState::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtSetDebugFilterState::globalNotifierPtr->Attach(instructionObserver);
    InstNtSetDebugFilterState::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtSetDebugFilterState::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ComponentId, ADDRINT Level, ADDRINT State) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar função
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar parâmetros
        const InstNtSetDebugFilterStateArgs* args = reinterpret_cast<InstNtSetDebugFilterStateArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtSetDebugFilterState Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        ComponentId: " << args->ComponentId << std::endl;
        stringStream << "        Level: " << args->Level << std::endl;
        stringStream << "        State: " << args->State << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(returnAddress);
        if (RTN_Valid(rtnCaller)) {
            LogRoutineInstructions(rtnCaller, stringStream, callContext->rtnAddress);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstNtSetDebugFilterState::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtSetDebugFilterState") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ComponentId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Level
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // State
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ComponentId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Level
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // State
            IARG_END);

        RTN_Close(rtn);
    }
}
