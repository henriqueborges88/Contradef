#include "InstNtQueryPerformanceCounter.h"

std::map<CallContextKey, CallContext*> InstNtQueryPerformanceCounter::callContextMap;
UINT32 InstNtQueryPerformanceCounter::imgCallId = 0;
UINT32 InstNtQueryPerformanceCounter::fcnCallId = 0;
Notifier* InstNtQueryPerformanceCounter::globalNotifierPtr;

VOID InstNtQueryPerformanceCounter::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtQueryPerformanceCounter::callContextMap, InstNtQueryPerformanceCounter::globalNotifierPtr);
}

VOID InstNtQueryPerformanceCounter::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtQueryPerformanceCounter::callContextMap, InstNtQueryPerformanceCounter::globalNotifierPtr);
}

VOID InstNtQueryPerformanceCounter::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtQueryPerformanceCounter::callContextMap, InstNtQueryPerformanceCounter::globalNotifierPtr);
}

VOID InstNtQueryPerformanceCounter::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT PerformanceCounter, ADDRINT PerformanceFrequency) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtQueryPerformanceCounterArgs args;
    args.PerformanceCounter = PerformanceCounter;
    args.PerformanceFrequency = PerformanceFrequency;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtQueryPerformanceCounter::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtQueryPerformanceCounter::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtQueryPerformanceCounter::globalNotifierPtr->Attach(instructionObserver);
    InstNtQueryPerformanceCounter::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtQueryPerformanceCounter::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT PerformanceCounter, ADDRINT PerformanceFrequency) {

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
        const InstNtQueryPerformanceCounterArgs* args = reinterpret_cast<InstNtQueryPerformanceCounterArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtQueryPerformanceCounter Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        PerformanceCounter: " << args->PerformanceCounter << std::endl;
        stringStream << "        PerformanceFrequency: " << args->PerformanceFrequency << std::endl;
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

VOID InstNtQueryPerformanceCounter::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtQueryPerformanceCounter") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // PerformanceCounter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // PerformanceFrequency
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // PerformanceCounter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // PerformanceFrequency
            IARG_END);

        RTN_Close(rtn);
    }
}