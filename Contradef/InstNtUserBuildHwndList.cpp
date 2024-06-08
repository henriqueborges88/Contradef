#include "InstNtUserBuildHwndList.h"

std::map<CallContextKey, CallContext*> InstNtUserBuildHwndList::callContextMap;
UINT32 InstNtUserBuildHwndList::imgCallId = 0;
UINT32 InstNtUserBuildHwndList::fcnCallId = 0;
Notifier* InstNtUserBuildHwndList::globalNotifierPtr;

VOID InstNtUserBuildHwndList::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtUserBuildHwndList::callContextMap, InstNtUserBuildHwndList::globalNotifierPtr);
}

VOID InstNtUserBuildHwndList::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtUserBuildHwndList::callContextMap, InstNtUserBuildHwndList::globalNotifierPtr);
}

VOID InstNtUserBuildHwndList::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtUserBuildHwndList::callContextMap, InstNtUserBuildHwndList::globalNotifierPtr);
}

VOID InstNtUserBuildHwndList::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hDesktop, ADDRINT hwndParent, ADDRINT hwndChild, ADDRINT bEnumChildren, ADDRINT pWnds, ADDRINT nWnds) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtUserBuildHwndListArgs args;
    args.hDesktop = hDesktop;
    args.hwndParent = hwndParent;
    args.hwndChild = hwndChild;
    args.bEnumChildren = bEnumChildren;
    args.pWnds = pWnds;
    args.nWnds = nWnds;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtUserBuildHwndList::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtUserBuildHwndList::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtUserBuildHwndList::globalNotifierPtr->Attach(instructionObserver);
    InstNtUserBuildHwndList::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtUserBuildHwndList::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT hDesktop, ADDRINT hwndParent, ADDRINT hwndChild, ADDRINT bEnumChildren, ADDRINT pWnds, ADDRINT nWnds) {

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
        const InstNtUserBuildHwndListArgs* args = reinterpret_cast<InstNtUserBuildHwndListArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hDesktop: " << args->hDesktop << std::endl;
        stringStream << "        hwndParent: " << args->hwndParent << std::endl;
        stringStream << "        hwndChild: " << args->hwndChild << std::endl;
        stringStream << "        bEnumChildren: " << args->bEnumChildren << std::endl;
        stringStream << "        pWnds: " << args->pWnds << std::endl;
        stringStream << "        nWnds: " << args->nWnds << std::endl;
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

VOID InstNtUserBuildHwndList::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtUserBuildHwndList" || rtnName == "ZwUserBuildHwndList") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hDesktop
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hwndChild
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // bEnumChildren
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // pWnds
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nWnds
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hDesktop
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hwndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hwndChild
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // bEnumChildren
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // pWnds
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nWnds
            IARG_END);

        RTN_Close(rtn);
    }
}
