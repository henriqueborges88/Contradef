#include "InstNtQueryInformationThread.h"

std::map<CallContextKey, CallContext*> InstNtQueryInformationThread::callContextMap;
UINT32 InstNtQueryInformationThread::imgCallId = 0;
UINT32 InstNtQueryInformationThread::fcnCallId = 0;
Notifier* InstNtQueryInformationThread::globalNotifierPtr;

VOID InstNtQueryInformationThread::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtQueryInformationThread::callContextMap, InstNtQueryInformationThread::globalNotifierPtr);
}

VOID InstNtQueryInformationThread::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtQueryInformationThread::callContextMap, InstNtQueryInformationThread::globalNotifierPtr);
}

VOID InstNtQueryInformationThread::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtQueryInformationThread::callContextMap, InstNtQueryInformationThread::globalNotifierPtr);
}

VOID InstNtQueryInformationThread::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadInformationClass, ADDRINT ThreadInformation,
    ADDRINT ThreadInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtQueryInformationThreadArgs args;
    args.ThreadHandle = ThreadHandle;
    args.ThreadInformationClass = ThreadInformationClass;
    args.ThreadInformation = ThreadInformation;
    args.ThreadInformationLength = ThreadInformationLength;
    args.ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtQueryInformationThread::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtQueryInformationThread::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtQueryInformationThread::globalNotifierPtr->Attach(instructionObserver);
    InstNtQueryInformationThread::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtQueryInformationThread::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT ThreadInformationClass, ADDRINT ThreadInformation,
    ADDRINT ThreadInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar par�metros
        const InstNtQueryInformationThreadArgs* args = reinterpret_cast<InstNtQueryInformationThreadArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        ThreadHandle: " << args->ThreadHandle << std::endl;
        stringStream << "        ThreadInformationClass: " << args->ThreadInformationClass << std::endl;
        stringStream << "        ThreadInformation: " << args->ThreadInformation << std::endl;
        stringStream << "        ThreadInformationLength: " << args->ThreadInformationLength << std::endl;
        stringStream << "        ReturnLength: " << args->ReturnLength << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instru��es da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(returnAddress);
        if (RTN_Valid(rtnCaller)) {
            LogRoutineInstructions(rtnCaller, stringStream, callContext->rtnAddress);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstNtQueryInformationThread::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtQueryInformationThread" || rtnName == "ZwQueryInformationThread") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ThreadInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ThreadInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ThreadInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ThreadInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ThreadInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_Close(rtn);
    }
}
