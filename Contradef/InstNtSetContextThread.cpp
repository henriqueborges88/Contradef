#include "InstNtSetContextThread.h"

std::map<CallContextKey, CallContext*> InstNtSetContextThread::callContextMap;
UINT32 InstNtSetContextThread::imgCallId = 0;
UINT32 InstNtSetContextThread::fcnCallId = 0;
Notifier* InstNtSetContextThread::globalNotifierPtr;

VOID InstNtSetContextThread::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtSetContextThread::callContextMap, InstNtSetContextThread::globalNotifierPtr);
}

VOID InstNtSetContextThread::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtSetContextThread::callContextMap, InstNtSetContextThread::globalNotifierPtr);
}

VOID InstNtSetContextThread::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtSetContextThread::callContextMap, InstNtSetContextThread::globalNotifierPtr);
}

VOID InstNtSetContextThread::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadContext) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtSetContextThreadArgs args;
    args.ThreadHandle = ThreadHandle;
    args.ThreadContext = ThreadContext;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtSetContextThread::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtSetContextThread::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtSetContextThread::globalNotifierPtr->Attach(instructionObserver);
    InstNtSetContextThread::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtSetContextThread::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadContext) {

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
        const InstNtSetContextThreadArgs* args = reinterpret_cast<InstNtSetContextThreadArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtSetContextThread Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        ThreadHandle: " << args->ThreadHandle << std::endl;
        stringStream << "        ThreadContext: " << args->ThreadContext << std::endl;
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

VOID InstNtSetContextThread::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtSetContextThread") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadContext
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadContext
            IARG_END);

        RTN_Close(rtn);
    }
}
