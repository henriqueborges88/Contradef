#include "InstBaseRegOpenKey.h"

std::map<CallContextKey, CallContext*> InstBaseRegOpenKey::callContextMap;
UINT32 InstBaseRegOpenKey::imgCallId = 0;
UINT32 InstBaseRegOpenKey::fcnCallId = 0;
Notifier* InstBaseRegOpenKey::globalNotifierPtr;

VOID InstBaseRegOpenKey::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstBaseRegOpenKey::callContextMap, InstBaseRegOpenKey::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstBaseRegOpenKey::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstBaseRegOpenKey::callContextMap, InstBaseRegOpenKey::globalNotifierPtr);
}

VOID InstBaseRegOpenKey::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstBaseRegOpenKey::callContextMap, InstBaseRegOpenKey::globalNotifierPtr);
}

VOID InstBaseRegOpenKey::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    BaseRegOpenKeyArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    //// CONTRAMEDIDA
    args.originalLastChar = *reinterpret_cast<wchar_t*>(lpSubKey + wcslen(reinterpret_cast<wchar_t*>(lpSubKey)) - 1); // Salvar o último caractere original de lpSubKey

    std::wstring wslpSubKey = ConvertAddrToWideString(lpSubKey);
    std::string ansiStringLpSubKey = WStringToString(wslpSubKey);
    if (isRegistryKeyPartInList(ansiStringLpSubKey)) {
        wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(lpSubKey);
        size_t len = wcslen(wideCharStr);

        if (len > 0) {
            wideCharStr[len - 1] = L'Ç';
            args.isModified = true;
        }
    }
    ////

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstBaseRegOpenKey::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstBaseRegOpenKey::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstBaseRegOpenKey::globalNotifierPtr->Attach(instructionObserver);
    InstBaseRegOpenKey::globalNotifierPtr->Attach(traceObserver);
}

VOID InstBaseRegOpenKey::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

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
        const BaseRegOpenKeyArgs* args = reinterpret_cast<BaseRegOpenKeyArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wslpSubKey = ConvertAddrToWideString(args->lpSubKey);

        ////CONTRAMEDIDA
        if (args->isModified) {
            wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(lpSubKey);
            size_t len = wcslen(wideCharStr);

            if (len > 0) {
                wideCharStr[len - 1] = args->originalLastChar;
            }
        }
        ////

        stringStream << std::endl << "[+] BaseRegOpenKey..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpSubKey: " << WStringToString(wslpSubKey) << std::endl;
        stringStream << "        phkResult: " << args->phkResult << std::endl;
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

VOID InstBaseRegOpenKey::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "BaseRegOpenKey") {
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
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }

}