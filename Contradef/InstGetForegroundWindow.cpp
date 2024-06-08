#include "InstGetForegroundWindow.h"

/*
A fun��o GetForegroundWindow n�o possui argumentos. Ela simplesmente retorna o identificador da janela atualmente em primeiro plano. Por isso, o m�todo CallbackBefore apenas registra a chamada da fun��o, e o CallbackAfter registra o valor de retorno.
*/

std::map<CallContextKey, CallContext*> InstGetForegroundWindow::callContextMap;
UINT32 InstGetForegroundWindow::imgCallId = 0;
UINT32 InstGetForegroundWindow::fcnCallId = 0;
Notifier* InstGetForegroundWindow::globalNotifierPtr;

VOID InstGetForegroundWindow::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstGetForegroundWindow::callContextMap, InstGetForegroundWindow::globalNotifierPtr);
}

// Callbacks que ser�o chamados quando um evento for notificado
VOID InstGetForegroundWindow::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstGetForegroundWindow::callContextMap, InstGetForegroundWindow::globalNotifierPtr);
}

VOID InstGetForegroundWindow::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstGetForegroundWindow::callContextMap, InstGetForegroundWindow::globalNotifierPtr);
}

VOID InstGetForegroundWindow::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress) {
    std::cout << "TEst" << std::endl;
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, NULL);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstGetForegroundWindow::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstGetForegroundWindow::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstGetForegroundWindow::globalNotifierPtr->Attach(instructionObserver);
    InstGetForegroundWindow::globalNotifierPtr->Attach(traceObserver);
}

VOID InstGetForegroundWindow::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress) {

    if (!IsMainExecutable(returnAddress)) {
       return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar par�metros
        std::stringstream& stringStream = callContext->stringStream;
        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
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

VOID InstGetForegroundWindow::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetForegroundWindow") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;
        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_END);

        RTN_Close(rtn);
    }
}
