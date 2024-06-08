#include "InstCoCreateInstance.h"

std::map<CallContextKey, CallContext*> InstCoCreateInstance::callContextMap;
UINT32 InstCoCreateInstance::imgCallId = 0;
UINT32 InstCoCreateInstance::fcnCallId = 0;
Notifier* InstCoCreateInstance::globalNotifierPtr;

VOID InstCoCreateInstance::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstCoCreateInstance::callContextMap, InstCoCreateInstance::globalNotifierPtr);
}

VOID InstCoCreateInstance::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstCoCreateInstance::callContextMap, InstCoCreateInstance::globalNotifierPtr);
}

VOID InstCoCreateInstance::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstCoCreateInstance::callContextMap, InstCoCreateInstance::globalNotifierPtr);
}

VOID InstCoCreateInstance::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT rclsid, ADDRINT pUnkOuter, ADDRINT dwClsContext, ADDRINT riid, ADDRINT ppv) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstCoCreateInstanceArgs args;
    args.rclsid = rclsid;
    args.pUnkOuter = pUnkOuter;
    args.dwClsContext = dwClsContext;
    args.riid = riid;
    args.ppv = ppv;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstCoCreateInstance::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstCoCreateInstance::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstCoCreateInstance::globalNotifierPtr->Attach(instructionObserver);
    InstCoCreateInstance::globalNotifierPtr->Attach(traceObserver);
}

VOID InstCoCreateInstance::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT rclsid, ADDRINT pUnkOuter, ADDRINT dwClsContext, ADDRINT riid, ADDRINT ppv) {

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
        const InstCoCreateInstanceArgs* args = reinterpret_cast<InstCoCreateInstanceArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        rclsid: " << args->rclsid << std::endl;
        stringStream << "        pUnkOuter: " << args->pUnkOuter << std::endl;
        stringStream << "        dwClsContext: " << args->dwClsContext << std::endl;
        stringStream << "        riid: " << args->riid << std::endl;
        stringStream << "        ppv: " << args->ppv << std::endl;
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

VOID InstCoCreateInstance::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CoCreateInstance") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // rclsid
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pUnkOuter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwClsContext
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // riid
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ppv
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // rclsid
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pUnkOuter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwClsContext
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // riid
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ppv
            IARG_END);

        RTN_Close(rtn);
    }
}
