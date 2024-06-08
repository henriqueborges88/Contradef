#include "InstNtSetInformationProcess.h"

std::map<CallContextKey, CallContext*> InstNtSetInformationProcess::callContextMap;
UINT32 InstNtSetInformationProcess::imgCallId = 0;
UINT32 InstNtSetInformationProcess::fcnCallId = 0;
Notifier* InstNtSetInformationProcess::globalNotifierPtr;

VOID InstNtSetInformationProcess::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtSetInformationProcess::callContextMap, InstNtSetInformationProcess::globalNotifierPtr);
}

VOID InstNtSetInformationProcess::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtSetInformationProcess::callContextMap, InstNtSetInformationProcess::globalNotifierPtr);
}

VOID InstNtSetInformationProcess::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtSetInformationProcess::callContextMap, InstNtSetInformationProcess::globalNotifierPtr);
}

VOID InstNtSetInformationProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtSetInformationProcessArgs args;
    args.ProcessHandle = ProcessHandle;
    args.ProcessInformationClass = ProcessInformationClass;
    args.ProcessInformation = ProcessInformation;
    args.ProcessInformationLength = ProcessInformationLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtSetInformationProcess::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtSetInformationProcess::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtSetInformationProcess::globalNotifierPtr->Attach(instructionObserver);
    InstNtSetInformationProcess::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtSetInformationProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength) {

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
        const InstNtSetInformationProcessArgs* args = reinterpret_cast<InstNtSetInformationProcessArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtSetInformationProcess Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        ProcessHandle: " << args->ProcessHandle << std::endl;
        stringStream << "        ProcessInformationClass: " << args->ProcessInformationClass << std::endl;
        stringStream << "        ProcessInformation: " << args->ProcessInformation << std::endl;
        stringStream << "        ProcessInformationLength: " << args->ProcessInformationLength << std::endl;
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

VOID InstNtSetInformationProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtSetInformationProcess") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
            IARG_END);

        RTN_Close(rtn);
    }
}