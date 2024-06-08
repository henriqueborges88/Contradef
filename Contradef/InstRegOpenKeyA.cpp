#include "InstRegOpenKeyA.h"

std::map<CallContextKey, CallContext*> InstRegOpenKeyA::callContextMap;
UINT32 InstRegOpenKeyA::imgCallId = 0;
UINT32 InstRegOpenKeyA::fcnCallId = 0;
Notifier* InstRegOpenKeyA::globalNotifierPtr;

VOID InstRegOpenKeyA::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

// Callbacks que ser�o chamados quando um evento for notificado
VOID InstRegOpenKeyA::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

VOID InstRegOpenKeyA::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

VOID InstRegOpenKeyA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegOpenKeyAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    //// CONTRAMEDIDA
    args.originalLastChar = *reinterpret_cast<char*>(lpSubKey + strlen(reinterpret_cast<char*>(lpSubKey)) - 1); // Salvar o �ltimo caractere original de lpSubKey

    std::string ansiStringLpSubKey = ConvertAddrToAnsiString(lpSubKey);
    if (isRegistryKeyPartInList(ansiStringLpSubKey)) {
        char* ansiCharStr = reinterpret_cast<char*>(lpSubKey);
        size_t len = strlen(ansiCharStr);

        if (len > 0) {
            ansiCharStr[len - 1] = '�';
            args.isModified = true;
        }
    }
    ////

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegOpenKeyA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegOpenKeyA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegOpenKeyA::globalNotifierPtr->Attach(instructionObserver);
    InstRegOpenKeyA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegOpenKeyA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
   
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
        const InstRegOpenKeyAArgs* args = reinterpret_cast<InstRegOpenKeyAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpSubKey = ConvertAddrToAnsiString(args->lpSubKey);
       
        ////CONTRAMEDIDA
        if (args->isModified) {
            char* ansiCharStr = reinterpret_cast<char*>(args->lpSubKey);
            size_t len = strlen(ansiCharStr);

            if (len > 0) {
                ansiCharStr[len - 1] = args->originalLastChar;
            }
        }
        ////

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpSubKey: " << ansiStringLpSubKey << std::endl;
        stringStream << "        phkResult: " << args->phkResult << std::endl;
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

VOID InstRegOpenKeyA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegOpenKeyA") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }
}

