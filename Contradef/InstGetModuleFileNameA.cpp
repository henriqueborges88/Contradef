#include "InstGetModuleFileNameA.h"

std::map<CallContextKey, CallContext*> InstGetModuleFileNameA::callContextMap;
UINT32 InstGetModuleFileNameA::imgCallId = 0;
UINT32 InstGetModuleFileNameA::fcnCallId = 0;
Notifier* InstGetModuleFileNameA::globalNotifierPtr;

VOID InstGetModuleFileNameA::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstGetModuleFileNameA::callContextMap, InstGetModuleFileNameA::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstGetModuleFileNameA::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstGetModuleFileNameA::callContextMap, InstGetModuleFileNameA::globalNotifierPtr);
}

VOID InstGetModuleFileNameA::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstGetModuleFileNameA::callContextMap, InstGetModuleFileNameA::globalNotifierPtr);
}

VOID InstGetModuleFileNameA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    GetModuleFileNameAArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstGetModuleFileNameA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstGetModuleFileNameA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstGetModuleFileNameA::globalNotifierPtr->Attach(instructionObserver);
    InstGetModuleFileNameA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstGetModuleFileNameA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar funçao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar parámetros
        const GetModuleFileNameAArgs* args = reinterpret_cast<GetModuleFileNameAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpFilename = ConvertAddrToAnsiString(args->lpFilename);
        stringStream << std::endl << "[+] GetModuleFileNameA..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hModule: " << hModule << std::endl;
        stringStream << "        lpFilename: " << ansiStringLpFilename << std::endl;
        stringStream << "        nSize: " << nSize << std::endl;
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

VOID InstGetModuleFileNameA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

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
    
    if (RTN_Name(rtn) == "GetModuleFileNameA") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFilename
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nSize
            IARG_END);

        RTN_Close(rtn);
    }

}