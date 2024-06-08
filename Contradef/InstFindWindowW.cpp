#include "InstFindWindowW.h"

std::map<CallContextKey, CallContext*> InstFindWindowW::callContextMap;
UINT32 InstFindWindowW::imgCallId = 0;
UINT32 InstFindWindowW::fcnCallId = 0;
Notifier* InstFindWindowW::globalNotifierPtr;

VOID InstFindWindowW::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstFindWindowW::callContextMap, InstFindWindowW::globalNotifierPtr);
}

VOID InstFindWindowW::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstFindWindowW::callContextMap, InstFindWindowW::globalNotifierPtr);
}

VOID InstFindWindowW::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstFindWindowW::callContextMap, InstFindWindowW::globalNotifierPtr);
}

VOID InstFindWindowW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpClassName, ADDRINT lpWindowName) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstFindWindowWArgs args;
    args.lpClassName = lpClassName;
    args.lpWindowName = lpWindowName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstFindWindowW::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstFindWindowW::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstFindWindowW::globalNotifierPtr->Attach(instructionObserver);
    InstFindWindowW::globalNotifierPtr->Attach(traceObserver);
}

VOID InstFindWindowW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT lpClassName, ADDRINT lpWindowName) {

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
        const InstFindWindowWArgs* args = reinterpret_cast<InstFindWindowWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring className = ConvertAddrToWideString(args->lpClassName);
        std::wstring windowName = ConvertAddrToWideString(args->lpWindowName);
        std::string ansiClassName(className.begin(), className.end());
        std::string ansiWindowName(windowName.begin(), windowName.end());

        stringStream << std::endl << "[+] FindWindowW Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        lpClassName: " << ansiClassName << std::endl;
        stringStream << "        lpWindowName: " << ansiWindowName << std::endl;
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

VOID InstFindWindowW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "FindWindowW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpWindowName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpWindowName
            IARG_END);

        RTN_Close(rtn);
    }
}