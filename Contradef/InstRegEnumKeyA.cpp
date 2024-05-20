#include "InstRegEnumKeyA.h"

std::map<CallContextKey, CallContext*> InstRegEnumKeyA::callContextMap;
UINT32 InstRegEnumKeyA::imgCallId = 0;
UINT32 InstRegEnumKeyA::fcnCallId = 0;
Notifier* InstRegEnumKeyA::globalNotifierPtr;

VOID InstRegEnumKeyA::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegEnumKeyA::callContextMap, InstRegEnumKeyA::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstRegEnumKeyA::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegEnumKeyA::callContextMap, InstRegEnumKeyA::globalNotifierPtr);
}

VOID InstRegEnumKeyA::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegEnumKeyA::callContextMap, InstRegEnumKeyA::globalNotifierPtr);
}

VOID InstRegEnumKeyA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName) {
    std::cout << "EnumKey Invocado" << std::endl;

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegEnumKeyAArgs args;
    args.hKey = hKey;
    args.dwIndex = dwIndex;
    args.lpName = lpName;
    args.lpcName = lpcName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegEnumKeyA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegEnumKeyA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegEnumKeyA::globalNotifierPtr->Attach(instructionObserver);
    InstRegEnumKeyA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegEnumKeyA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName) {

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
        const InstRegEnumKeyAArgs* args = reinterpret_cast<InstRegEnumKeyAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpName = ConvertAddrToAnsiString(args->lpName);

        stringStream << std::endl << "[+] RegEnumKeyA..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        dwIndex: " << args->dwIndex << std::endl;
        stringStream << "        lpName: " << ansiStringLpName << std::endl;
        stringStream << "        lpcName: " << args->lpcName << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        if (isRegistryKeyPartInList(ansiStringLpName)) {
            std::cout << "A chave de registro está na lista." << std::endl;
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //*retValAddr = 0;
            //alterar o valor de retorno
            //verificar a necessidade de chamar RegCloseKey para encerrar o handle
            //se possível, alterar o valor da chave de registro antes da chamada, colocar uma chave que não existe
        }
        else {
            std::cout << "A chave de registro não está na lista." << std::endl;
        }

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

VOID InstRegEnumKeyA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "RegEnumKeyA") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
            IARG_END);

        RTN_Close(rtn);
    }
}
