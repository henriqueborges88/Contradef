#include "InstRegOpenKeyExW.h"

std::map<CallContextKey, CallContext*> InstRegOpenKeyExW::callContextMap;
UINT32 InstRegOpenKeyExW::imgCallId = 0;
UINT32 InstRegOpenKeyExW::fcnCallId = 0;
Notifier* InstRegOpenKeyExW::globalNotifierPtr;

VOID InstRegOpenKeyExW::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegOpenKeyExW::callContextMap, InstRegOpenKeyExW::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstRegOpenKeyExW::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegOpenKeyExW::callContextMap, InstRegOpenKeyExW::globalNotifierPtr);
}

VOID InstRegOpenKeyExW::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegOpenKeyExW::callContextMap, InstRegOpenKeyExW::globalNotifierPtr);
}

VOID InstRegOpenKeyExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    RegOpenKeyExWArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.ulOptions = ulOptions;
    args.samDesired = samDesired;
    args.phkResult = phkResult;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegOpenKeyExW::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegOpenKeyExW::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegOpenKeyExW::globalNotifierPtr->Attach(instructionObserver);
    InstRegOpenKeyExW::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegOpenKeyExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {

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
        const RegOpenKeyExWArgs* args = reinterpret_cast<RegOpenKeyExWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringLpSubKey = ConvertAddrToWideString(args->lpSubKey);
        stringStream << std::endl << "[+] RegOpenKeyExW..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpSubKey: " << WStringToString(wideStringLpSubKey) << std::endl;
        stringStream << "        ulOptions: " << args->ulOptions << std::endl;
        stringStream << "        samDesired: " << args->samDesired << std::endl;
        stringStream << "        phkResult: " << args->phkResult << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        if (isRegistryKeyPartInList(WStringToString(wideStringLpSubKey))) {
            std::cout << "A chave de registro está na lista." << std::endl;
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //*retValAddr = 0;
            //alterar o valor de retorno
            //verificar a necessidade de chamar RegCloseKey para encerrar o handle
            //se possivel, alterar o valor da chave de registro antes da chamada, colocar uma chave que não existe
        }
        else {
            std::cout << "A chave de registro não está na lista." << std::endl;
        }

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

VOID InstRegOpenKeyExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

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

    if (RTN_Name(rtn) == "RegOpenKeyExW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }
}