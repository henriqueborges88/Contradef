#include "InstCheckRemoteDebuggerPresent.h"

std::map<CallContextKey, CallContext*> InstCheckRemoteDebuggerPresent::callContextMap;
UINT32 InstCheckRemoteDebuggerPresent::imgCallId = 0;
UINT32 InstCheckRemoteDebuggerPresent::fcnCallId = 0;
Notifier* InstCheckRemoteDebuggerPresent::globalNotifierPtr;

VOID InstCheckRemoteDebuggerPresent::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstCheckRemoteDebuggerPresent::callContextMap, InstCheckRemoteDebuggerPresent::globalNotifierPtr);
}

// Callbacks que ser�o chamados quando um evento for notificado
VOID InstCheckRemoteDebuggerPresent::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstCheckRemoteDebuggerPresent::callContextMap, InstCheckRemoteDebuggerPresent::globalNotifierPtr);
}

VOID InstCheckRemoteDebuggerPresent::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstCheckRemoteDebuggerPresent::callContextMap, InstCheckRemoteDebuggerPresent::globalNotifierPtr);
}

VOID InstCheckRemoteDebuggerPresent::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstCheckRemoteDebuggerPresentArgs args;
    args.hProcess = hProcess;
    args.pbDebuggerPresent = pbDebuggerPresent;

    // Verificar por que esta declara��o mantem o endere�o correto de pbDebuggerPresent no CB CallbackAfter 
    BOOL* debuggerPresent = reinterpret_cast<BOOL*>(args.pbDebuggerPresent);

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstCheckRemoteDebuggerPresent::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstCheckRemoteDebuggerPresent::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstCheckRemoteDebuggerPresent::globalNotifierPtr->Attach(instructionObserver);
    InstCheckRemoteDebuggerPresent::globalNotifierPtr->Attach(traceObserver);
}

VOID InstCheckRemoteDebuggerPresent::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {
    
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
        const InstCheckRemoteDebuggerPresentArgs* args = reinterpret_cast<InstCheckRemoteDebuggerPresentArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        BOOL* debuggerPresent = reinterpret_cast<BOOL*>(args->pbDebuggerPresent);
        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hProcess: " << args->hProcess << std::endl;
        stringStream << "        pbDebuggerPresent: " << *debuggerPresent << std::endl;
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

VOID InstCheckRemoteDebuggerPresent::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    SEC sec = RTN_Sec(rtn);
    IMG img = SEC_Img(sec);
    if (IMG_Valid(img)) {
        std::string imageName = IMG_Name(img);
        std::string moduleName = ExtractModuleName(IMG_Name(img));
        if (toUpperCase(moduleName) != "KERNELBASE.DLL") {
            return;
        }
    }
    else {
        return;
    }

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CheckRemoteDebuggerPresent") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
            IARG_END);
        RTN_Close(rtn);
    }

}