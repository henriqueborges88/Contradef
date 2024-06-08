#include "InstRegEnumKeyExW.h"

std::map<CallContextKey, CallContext*> InstRegEnumKeyExW::callContextMap;
UINT32 InstRegEnumKeyExW::imgCallId = 0;
UINT32 InstRegEnumKeyExW::fcnCallId = 0;
Notifier* InstRegEnumKeyExW::globalNotifierPtr;

VOID InstRegEnumKeyExW::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegEnumKeyExW::callContextMap, InstRegEnumKeyExW::globalNotifierPtr);
}

// Callbacks que ser�o chamados quando um evento for notificado
VOID InstRegEnumKeyExW::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegEnumKeyExW::callContextMap, InstRegEnumKeyExW::globalNotifierPtr);
}

VOID InstRegEnumKeyExW::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegEnumKeyExW::callContextMap, InstRegEnumKeyExW::globalNotifierPtr);
}

VOID InstRegEnumKeyExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName, ADDRINT lpReserved, ADDRINT lpClass, ADDRINT lpcClass, ADDRINT lpftLastWriteTime) {
    std::cout << "EnumKey Invocado" << std::endl;

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegEnumKeyExWArgs args;
    args.hKey = hKey;
    args.dwIndex = dwIndex;
    args.lpName = lpName;
    args.lpcName = lpcName;
    args.lpReserved = lpReserved;
    args.lpClass = lpClass;
    args.lpcClass = lpcClass;
    args.lpftLastWriteTime = lpftLastWriteTime;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegEnumKeyExW::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegEnumKeyExW::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegEnumKeyExW::globalNotifierPtr->Attach(instructionObserver);
    InstRegEnumKeyExW::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegEnumKeyExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName, ADDRINT lpReserved, ADDRINT lpClass, ADDRINT lpcClass, ADDRINT lpftLastWriteTime) {

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
        const InstRegEnumKeyExWArgs* args = reinterpret_cast<InstRegEnumKeyExWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringLpName = ConvertAddrToWideString(args->lpName);
        std::string ansiStringLpName(wideStringLpName.begin(), wideStringLpName.end());

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        dwIndex: " << args->dwIndex << std::endl;
        stringStream << "        lpName: " << ansiStringLpName << std::endl;
        stringStream << "        lpcName: " << args->lpcName << std::endl;
        stringStream << "        lpReserved: " << args->lpReserved << std::endl;
        stringStream << "        lpClass: " << args->lpClass << std::endl;
        stringStream << "        lpcClass: " << args->lpcClass << std::endl;
        stringStream << "        lpftLastWriteTime: " << args->lpftLastWriteTime << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        if (isRegistryKeyPartInList(ansiStringLpName)) {
            std::cout << "A chave de registro est� na lista." << std::endl;
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //*retValAddr = 0;
            //alterar o valor de retorno
            //verificar a necessidade de chamar RegCloseKey para encerrar o handle
            //se poss�vel, alterar o valor da chave de registro antes da chamada, colocar uma chave que n�o existe
        }
        else {
            std::cout << "A chave de registro n�o est� na lista." << std::endl;
        }

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

VOID InstRegEnumKeyExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegEnumKeyExW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpcClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpftLastWriteTime
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpcClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpftLastWriteTime
            IARG_END);

        RTN_Close(rtn);
    }
}
