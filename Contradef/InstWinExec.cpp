#include "InstWinExec.h"

std::map<CallContextKey, CallContext*> InstWinExec::callContextMap;
UINT32 InstWinExec::imgCallId = 0;
UINT32 InstWinExec::fcnCallId = 0;
Notifier* InstWinExec::globalNotifierPtr;

VOID InstWinExec::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstWinExec::callContextMap, InstWinExec::globalNotifierPtr);
}

VOID InstWinExec::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstWinExec::callContextMap, InstWinExec::globalNotifierPtr);
}

VOID InstWinExec::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstWinExec::callContextMap, InstWinExec::globalNotifierPtr);
}

VOID InstWinExec::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpCmdLine, ADDRINT uCmdShow) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstWinExecArgs args;
    args.lpCmdLine = lpCmdLine;
    args.uCmdShow = uCmdShow;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstWinExec::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstWinExec::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstWinExec::globalNotifierPtr->Attach(instructionObserver);
    InstWinExec::globalNotifierPtr->Attach(traceObserver);
}

VOID InstWinExec::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT lpCmdLine, ADDRINT uCmdShow) {

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
        const InstWinExecArgs* args = reinterpret_cast<InstWinExecArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string cmdLine = args->lpCmdLine ? ConvertAddrToAnsiString(args->lpCmdLine) : "NULL";

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        lpCmdLine: " << cmdLine << std::endl;
        stringStream << "        uCmdShow: " << args->uCmdShow << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instru��es da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(*retValAddr);
        if (RTN_Valid(rtnCaller)) {
            LogRoutineInstructions(rtnCaller, stringStream, callContext->rtnAddress);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstWinExec::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (RTN_Name(rtn) == "WinExec") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpCmdLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // uCmdShow
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpCmdLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // uCmdShow
            IARG_END);

        RTN_Close(rtn);
    }
}
