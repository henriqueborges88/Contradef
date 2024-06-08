#include "InstNtQueryObject.h"

std::map<CallContextKey, CallContext*> InstNtQueryObject::callContextMap;
UINT32 InstNtQueryObject::imgCallId = 0;
UINT32 InstNtQueryObject::fcnCallId = 0;
Notifier* InstNtQueryObject::globalNotifierPtr;

VOID InstNtQueryObject::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtQueryObject::callContextMap, InstNtQueryObject::globalNotifierPtr);
}

VOID InstNtQueryObject::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtQueryObject::callContextMap, InstNtQueryObject::globalNotifierPtr);
}

VOID InstNtQueryObject::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtQueryObject::callContextMap, InstNtQueryObject::globalNotifierPtr);
}

VOID InstNtQueryObject::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT Handle, ADDRINT ObjectInformationClass, ADDRINT ObjectInformation, ADDRINT ObjectInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtQueryObjectArgs args;
    args.Handle = Handle;
    args.ObjectInformationClass = ObjectInformationClass;
    args.ObjectInformation = ObjectInformation;
    args.ObjectInformationLength = ObjectInformationLength;
    args.ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtQueryObject::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtQueryObject::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtQueryObject::globalNotifierPtr->Attach(instructionObserver);
    InstNtQueryObject::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtQueryObject::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT Handle, ADDRINT ObjectInformationClass, ADDRINT ObjectInformation, ADDRINT ObjectInformationLength, ADDRINT ReturnLength) {

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
        const InstNtQueryObjectArgs* args = reinterpret_cast<InstNtQueryObjectArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtQueryObject Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        Handle: " << args->Handle << std::endl;
        stringStream << "        ObjectInformationClass: " << args->ObjectInformationClass << std::endl;
        stringStream << "        ObjectInformation: " << args->ObjectInformation << std::endl;
        stringStream << "        ObjectInformationLength: " << args->ObjectInformationLength << std::endl;
        stringStream << "        ReturnLength: " << args->ReturnLength << std::endl;
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

VOID InstNtQueryObject::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtQueryObject") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ObjectInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ObjectInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ObjectInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ObjectInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_Close(rtn);
    }
}

