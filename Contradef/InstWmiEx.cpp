#include "InstWmiEx.h"

std::map<CallContextKey, CallContext*> InstWmiEx::callContextMap;
UINT32 InstWmiEx::imgCallId = 0;
UINT32 InstWmiEx::fcnCallId = 0;
Notifier* InstWmiEx::globalNotifierPtr;

VOID InstWmiEx::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstWmiEx::callContextMap, InstWmiEx::globalNotifierPtr);
}

VOID InstWmiEx::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstWmiEx::callContextMap, InstWmiEx::globalNotifierPtr);
}

VOID InstWmiEx::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstWmiEx::callContextMap, InstWmiEx::globalNotifierPtr);
}

VOID InstWmiEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstWmiExArgs args;
    args.arg1 = arg1;
    args.arg2 = arg2;
    args.arg3 = arg3;
    args.arg4 = arg4;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstWmiEx::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstWmiEx::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstWmiEx::globalNotifierPtr->Attach(instructionObserver);
    InstWmiEx::globalNotifierPtr->Attach(traceObserver);
}

VOID InstWmiEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {

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
        const InstWmiExArgs* args = reinterpret_cast<InstWmiExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] WMI Call Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        arg1: " << args->arg1 << std::endl;
        stringStream << "        arg2: " << args->arg2 << std::endl;
        stringStream << "        arg3: " << args->arg3 << std::endl;
        stringStream << "        arg4: " << args->arg4 << std::endl;
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

VOID InstWmiEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "CoCreateInstanceEx" || RTN_Name(rtn) == "ConnectServer" || RTN_Name(rtn) == "ExecQuery" || RTN_Name(rtn) == "GetObject" || RTN_Name(rtn) == "GetObjectA" || RTN_Name(rtn) == "GetObjectW") {
        std::cout << "WmiEx Invocado " << RTN_Name(rtn) << std::endl;

        
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // arg1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // arg2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // arg3
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // arg4
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // arg1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // arg2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // arg3
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // arg4
            IARG_END);

        RTN_Close(rtn);
    }
}
