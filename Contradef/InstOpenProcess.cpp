#include "InstOpenProcess.h"

std::map<CallContextKey, CallContext*> InstOpenProcess::callContextMap;
UINT32 InstOpenProcess::imgCallId = 0;
UINT32 InstOpenProcess::fcnCallId = 0;
Notifier* InstOpenProcess::globalNotifierPtr;

VOID InstOpenProcess::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstOpenProcess::callContextMap, InstOpenProcess::globalNotifierPtr);
}

VOID InstOpenProcess::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstOpenProcess::callContextMap, InstOpenProcess::globalNotifierPtr);
}

VOID InstOpenProcess::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstOpenProcess::callContextMap, InstOpenProcess::globalNotifierPtr);
}

VOID InstOpenProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT dwDesiredAccess, ADDRINT bInheritHandle, ADDRINT dwProcessId) {
    
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstOpenProcessArgs args;
    args.dwDesiredAccess = dwDesiredAccess;
    args.bInheritHandle = bInheritHandle;
    args.dwProcessId = dwProcessId;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstOpenProcess::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstOpenProcess::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstOpenProcess::globalNotifierPtr->Attach(instructionObserver);
    InstOpenProcess::globalNotifierPtr->Attach(traceObserver);
}

VOID InstOpenProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT dwDesiredAccess, ADDRINT bInheritHandle, ADDRINT dwProcessId) {

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
        const InstOpenProcessArgs* args = reinterpret_cast<InstOpenProcessArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] OpenProcess Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        dwDesiredAccess: " << args->dwDesiredAccess << std::endl;
        stringStream << "        bInheritHandle: " << args->bInheritHandle << std::endl;
        stringStream << "        dwProcessId: " << args->dwProcessId << std::endl;
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

VOID InstOpenProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "OpenProcess") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // bInheritHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwProcessId
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // bInheritHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwProcessId
            IARG_END);

        RTN_Close(rtn);
    }
}
