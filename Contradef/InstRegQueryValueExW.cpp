#include "InstRegQueryValueExW.h"

std::map<CallContextKey, CallContext*> InstRegQueryValueExW::callContextMap;
UINT32 InstRegQueryValueExW::imgCallId = 0;
UINT32 InstRegQueryValueExW::fcnCallId = 0;
Notifier* InstRegQueryValueExW::globalNotifierPtr;

VOID InstRegQueryValueExW::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegQueryValueExW::callContextMap, InstRegQueryValueExW::globalNotifierPtr);
}

// Callbacks que ser�o chamados quando um evento for notificado
VOID InstRegQueryValueExW::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegQueryValueExW::callContextMap, InstRegQueryValueExW::globalNotifierPtr);
}

VOID InstRegQueryValueExW::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegQueryValueExW::callContextMap, InstRegQueryValueExW::globalNotifierPtr);
}

VOID InstRegQueryValueExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegQueryValueExWArgs args;
    args.hKey = hKey;
    args.lpValueName = lpValueName;
    args.lpReserved = lpReserved;
    args.lpType = lpType;
    args.lpData = lpData;
    args.lpcbData = lpcbData;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegQueryValueExW::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegQueryValueExW::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegQueryValueExW::globalNotifierPtr->Attach(instructionObserver);
    InstRegQueryValueExW::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegQueryValueExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {

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
        const InstRegQueryValueExWArgs* args = reinterpret_cast<InstRegQueryValueExWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringlpValueName = ConvertAddrToWideString(args->lpValueName);
        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpValueName: " << WStringToString(wideStringlpValueName) << std::endl;
        stringStream << "        lpReserved: " << args->lpReserved << std::endl;
        stringStream << "        lpType: " << args->lpType << std::endl;
        stringStream << "        lpData: " << args->lpData << std::endl;
        stringStream << "        lpcbData: " << args->lpcbData << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instru��es da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(returnAddress);
        if (RTN_Valid(rtnCaller)) {
            LogRoutineInstructions(rtnCaller, stringStream, callContext->rtnAddress);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstRegQueryValueExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

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

   std::string rtnName = RTN_Name(rtn);
    if (RTN_Name(rtn) == "GetModuleHandleA") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
            IARG_END);

        RTN_Close(rtn);
    }
}