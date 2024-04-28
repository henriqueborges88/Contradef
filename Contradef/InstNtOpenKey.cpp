#include "InstNtOpenKey.h"

std::map<CallContextKey, CallContext*> InstNtOpenKey::callContextMap;
UINT32 InstNtOpenKey::imgCallId = 0;
UINT32 InstNtOpenKey::fcnCallId = 0;
Notifier* InstNtOpenKey::globalNotifierPtr;

VOID InstNtOpenKey::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtOpenKey::callContextMap, InstNtOpenKey::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstNtOpenKey::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtOpenKey::callContextMap, InstNtOpenKey::globalNotifierPtr);
}

VOID InstNtOpenKey::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtOpenKey::callContextMap, InstNtOpenKey::globalNotifierPtr);
}

VOID InstNtOpenKey::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    NtOpenKeyArgs args;
    args.KeyHandle = KeyHandle;
    args.DesiredAccess = DesiredAccess;
    args.ObjectAttributes = ObjectAttributes;

    // Acessando a estrutura OBJECT_ATTRIBUTES
    OBJECT_ATTRIBUTES* objAttr = reinterpret_cast<OBJECT_ATTRIBUTES*>(ObjectAttributes);
    if (objAttr != nullptr && objAttr->ObjectName != nullptr) {
        UNICODE_STRING* unicodeStr = objAttr->ObjectName;
        std::wstring keyPath(unicodeStr->Buffer, unicodeStr->Length / sizeof(WCHAR));
        args.keyPathStr = WStringToString(keyPath);
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtOpenKey::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtOpenKey::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtOpenKey::globalNotifierPtr->Attach(instructionObserver);
    InstNtOpenKey::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtOpenKey::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar funçao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar parámetros
        const NtOpenKeyArgs* args = reinterpret_cast<NtOpenKeyArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << std::endl << "[+] NtOpenKey..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        KeyHandle: " << args->KeyHandle << std::endl;
        stringStream << "        keyPath: " << args->keyPathStr << std::endl;
        stringStream << "        DesiredAccess: " << args->DesiredAccess << std::endl;
        stringStream << "        ObjectAttributes: " << args->ObjectAttributes << std::endl;
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

VOID InstNtOpenKey::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {
    
    if (RTN_Name(rtn) == "NtOpenKey") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_END);
        RTN_Close(rtn);
    }

}