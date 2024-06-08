#include "InstNtUserFindWindowEx.h"

std::map<CallContextKey, CallContext*> InstNtUserFindWindowEx::callContextMap;
UINT32 InstNtUserFindWindowEx::imgCallId = 0;
UINT32 InstNtUserFindWindowEx::fcnCallId = 0;
Notifier* InstNtUserFindWindowEx::globalNotifierPtr;

VOID InstNtUserFindWindowEx::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtUserFindWindowEx::callContextMap, InstNtUserFindWindowEx::globalNotifierPtr);
}

VOID InstNtUserFindWindowEx::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtUserFindWindowEx::callContextMap, InstNtUserFindWindowEx::globalNotifierPtr);
}

VOID InstNtUserFindWindowEx::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtUserFindWindowEx::callContextMap, InstNtUserFindWindowEx::globalNotifierPtr);
}

VOID InstNtUserFindWindowEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hwndParent, ADDRINT hwndChildAfter, ADDRINT lpszClass, ADDRINT lpszWindow) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtUserFindWindowExArgs args;
    args.hwndParent = hwndParent;
    args.hwndChildAfter = hwndChildAfter;
    args.lpszClass = lpszClass;
    args.lpszWindow = lpszWindow;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtUserFindWindowEx::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtUserFindWindowEx::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtUserFindWindowEx::globalNotifierPtr->Attach(instructionObserver);
    InstNtUserFindWindowEx::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtUserFindWindowEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT hwndParent, ADDRINT hwndChildAfter, ADDRINT lpszClass, ADDRINT lpszWindow) {

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
        const InstNtUserFindWindowExArgs* args = reinterpret_cast<InstNtUserFindWindowExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string className = args->lpszClass ? ConvertAddrToAnsiString(args->lpszClass) : "NULL";
        std::string windowName = args->lpszWindow ? ConvertAddrToAnsiString(args->lpszWindow) : "NULL";

        stringStream << std::endl << "[+] NtUserFindWindowEx Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        hwndParent: " << args->hwndParent << std::endl;
        stringStream << "        hwndChildAfter: " << args->hwndChildAfter << std::endl;
        stringStream << "        lpszClass: " << className << std::endl;
        stringStream << "        lpszWindow: " << windowName << std::endl;
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

VOID InstNtUserFindWindowEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtUserFindWindowEx") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndChildAfter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpszClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpszWindow
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndChildAfter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpszClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpszWindow
            IARG_END);

        RTN_Close(rtn);
    }
}
