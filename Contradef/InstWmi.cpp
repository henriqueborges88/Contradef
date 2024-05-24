#include "InstWmi.h"

std::map<CallContextKey, CallContext*> InstWmi::callContextMap;
UINT32 InstWmi::imgCallId = 0;
UINT32 InstWmi::fcnCallId = 0;
Notifier* InstWmi::globalNotifierPtr;

VOID InstWmi::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstWmi::callContextMap, InstWmi::globalNotifierPtr);
}

VOID InstWmi::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstWmi::callContextMap, InstWmi::globalNotifierPtr);
}

VOID InstWmi::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstWmi::callContextMap, InstWmi::globalNotifierPtr);
}

VOID InstWmi::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {
    std::cout << "Wmi Invocado" << std::endl;

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstWmiArgs args;
    args.arg1 = arg1;
    args.arg2 = arg2;
    args.arg3 = arg3;
    args.arg4 = arg4;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstWmi::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstWmi::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstWmi::globalNotifierPtr->Attach(instructionObserver);
    InstWmi::globalNotifierPtr->Attach(traceObserver);
}

VOID InstWmi::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4) {

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
        const InstWmiArgs* args = reinterpret_cast<InstWmiArgs*>(callContext->functionArgs);
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

VOID InstWmi::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {
    std::cout << "Wmi registrado" << RTN_Name(rtn) << std::endl;

    if (RTN_Name(rtn) == "CoCreateInstance" || RTN_Name(rtn) == "ConnectServer" || RTN_Name(rtn) == "ExecQuery") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // arg1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // arg2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // arg3
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // arg4
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
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
