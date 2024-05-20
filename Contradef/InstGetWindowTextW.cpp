#include "InstGetWindowTextW.h"

std::map<CallContextKey, CallContext*> InstGetWindowTextW::callContextMap;
UINT32 InstGetWindowTextW::imgCallId = 0;
UINT32 InstGetWindowTextW::fcnCallId = 0;
Notifier* InstGetWindowTextW::globalNotifierPtr;

VOID InstGetWindowTextW::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstGetWindowTextW::callContextMap, InstGetWindowTextW::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstGetWindowTextW::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstGetWindowTextW::callContextMap, InstGetWindowTextW::globalNotifierPtr);
}

VOID InstGetWindowTextW::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstGetWindowTextW::callContextMap, InstGetWindowTextW::globalNotifierPtr);
}

VOID InstGetWindowTextW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    GetWindowTextWArgs args;
    args.hWnd = hWnd;
    args.lpString = lpString;
    args.nMaxCount = nMaxCount;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstGetWindowTextW::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstGetWindowTextW::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstGetWindowTextW::globalNotifierPtr->Attach(instructionObserver);
    InstGetWindowTextW::globalNotifierPtr->Attach(traceObserver);
}

VOID InstGetWindowTextW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount) {

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
        const GetWindowTextWArgs* args = reinterpret_cast<GetWindowTextWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringlpString = ConvertAddrToWideString(args->lpString);
        stringStream << std::endl << "[+] GetWindowTextW..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hWnd: " << args->hWnd << std::endl;
        stringStream << "        lpString: " << WStringToString(wideStringlpString) << std::endl;
        stringStream << "        nMaxCount: " << args->nMaxCount << std::endl;
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

VOID InstGetWindowTextW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "GetWindowTextW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nMaxCount
            IARG_END);

        RTN_Close(rtn);
    }

}
