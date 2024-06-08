#include "InstNtYieldExecution.h"

std::map<CallContextKey, CallContext*> InstNtYieldExecution::callContextMap;
UINT32 InstNtYieldExecution::imgCallId = 0;
UINT32 InstNtYieldExecution::fcnCallId = 0;
Notifier* InstNtYieldExecution::globalNotifierPtr;

VOID InstNtYieldExecution::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtYieldExecution::callContextMap, InstNtYieldExecution::globalNotifierPtr);
}

VOID InstNtYieldExecution::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtYieldExecution::callContextMap, InstNtYieldExecution::globalNotifierPtr);
}

VOID InstNtYieldExecution::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtYieldExecution::callContextMap, InstNtYieldExecution::globalNotifierPtr);
}

VOID InstNtYieldExecution::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, nullptr);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtYieldExecution::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtYieldExecution::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtYieldExecution::globalNotifierPtr->Attach(instructionObserver);
    InstNtYieldExecution::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtYieldExecution::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress) {

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
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtYieldExecution Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(*retValAddr);
        if (RTN_Valid(rtnCaller)) {
            LogRoutineInstructions(rtnCaller, stringStream, callContext->rtnAddress);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstNtYieldExecution::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtYieldExecution") {
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
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_END);

        RTN_Close(rtn);
    }
}