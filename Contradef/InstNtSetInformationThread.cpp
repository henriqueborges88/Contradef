#include "InstNtSetInformationThread.h"

std::map<CallContextKey, CallContext*> InstNtSetInformationThread::callContextMap;
UINT32 InstNtSetInformationThread::imgCallId = 0;
UINT32 InstNtSetInformationThread::fcnCallId = 0;
Notifier* InstNtSetInformationThread::globalNotifierPtr;

VOID InstNtSetInformationThread::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtSetInformationThread::callContextMap, InstNtSetInformationThread::globalNotifierPtr);
}

VOID InstNtSetInformationThread::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtSetInformationThread::callContextMap, InstNtSetInformationThread::globalNotifierPtr);
}

VOID InstNtSetInformationThread::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtSetInformationThread::callContextMap, InstNtSetInformationThread::globalNotifierPtr);
}

VOID InstNtSetInformationThread::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadInformationClass, ADDRINT ThreadInformation, ADDRINT ThreadInformationLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtSetInformationThreadArgs args;
    args.ThreadHandle = ThreadHandle;
    args.ThreadInformationClass = ThreadInformationClass;
    args.ThreadInformation = ThreadInformation;
    args.ThreadInformationLength = ThreadInformationLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtSetInformationThread::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtSetInformationThread::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtSetInformationThread::globalNotifierPtr->Attach(instructionObserver);
    InstNtSetInformationThread::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtSetInformationThread::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadInformationClass, ADDRINT ThreadInformation, ADDRINT ThreadInformationLength) {

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
        const InstNtSetInformationThreadArgs* args = reinterpret_cast<InstNtSetInformationThreadArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtSetInformationThread Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        ThreadHandle: " << args->ThreadHandle << std::endl;
        stringStream << "        ThreadInformationClass: " << args->ThreadInformationClass << std::endl;
        stringStream << "        ThreadInformation: " << args->ThreadInformation << std::endl;
        stringStream << "        ThreadInformationLength: " << args->ThreadInformationLength << std::endl;
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

VOID InstNtSetInformationThread::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtSetInformationThread") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ThreadInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ThreadInformationLength
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ThreadInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ThreadInformationLength
            IARG_END);

        RTN_Close(rtn);
    }
}
