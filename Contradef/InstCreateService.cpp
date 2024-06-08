#include "InstCreateService.h"

std::map<CallContextKey, CallContext*> InstCreateService::callContextMap;
UINT32 InstCreateService::imgCallId = 0;
UINT32 InstCreateService::fcnCallId = 0;
Notifier* InstCreateService::globalNotifierPtr;

VOID InstCreateService::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstCreateService::callContextMap, InstCreateService::globalNotifierPtr);
}

VOID InstCreateService::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstCreateService::callContextMap, InstCreateService::globalNotifierPtr);
}

VOID InstCreateService::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstCreateService::callContextMap, InstCreateService::globalNotifierPtr);
}

VOID InstCreateService::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hSCManager, ADDRINT lpServiceName, ADDRINT lpDisplayName, ADDRINT dwDesiredAccess,
    ADDRINT dwServiceType, ADDRINT dwStartType, ADDRINT dwErrorControl, ADDRINT lpBinaryPathName,
    ADDRINT lpLoadOrderGroup, ADDRINT lpdwTagId, ADDRINT lpDependencies, ADDRINT lpServiceStartName,
    ADDRINT lpPassword) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstCreateServiceArgs args;
    args.hSCManager = hSCManager;
    args.lpServiceName = lpServiceName;
    args.lpDisplayName = lpDisplayName;
    args.dwDesiredAccess = dwDesiredAccess;
    args.dwServiceType = dwServiceType;
    args.dwStartType = dwStartType;
    args.dwErrorControl = dwErrorControl;
    args.lpBinaryPathName = lpBinaryPathName;
    args.lpLoadOrderGroup = lpLoadOrderGroup;
    args.lpdwTagId = lpdwTagId;
    args.lpDependencies = lpDependencies;
    args.lpServiceStartName = lpServiceStartName;
    args.lpPassword = lpPassword;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstCreateService::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstCreateService::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstCreateService::globalNotifierPtr->Attach(instructionObserver);
    InstCreateService::globalNotifierPtr->Attach(traceObserver);
}

VOID InstCreateService::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT hSCManager, ADDRINT lpServiceName, ADDRINT lpDisplayName, ADDRINT dwDesiredAccess,
    ADDRINT dwServiceType, ADDRINT dwStartType, ADDRINT dwErrorControl, ADDRINT lpBinaryPathName,
    ADDRINT lpLoadOrderGroup, ADDRINT lpdwTagId, ADDRINT lpDependencies, ADDRINT lpServiceStartName,
    ADDRINT lpPassword) {

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
        const InstCreateServiceArgs* args = reinterpret_cast<InstCreateServiceArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string serviceName = args->lpServiceName ? ConvertAddrToAnsiString(args->lpServiceName) : "NULL";
        std::string displayName = args->lpDisplayName ? ConvertAddrToAnsiString(args->lpDisplayName) : "NULL";
        std::string binaryPathName = args->lpBinaryPathName ? ConvertAddrToAnsiString(args->lpBinaryPathName) : "NULL";
        std::string serviceStartName = args->lpServiceStartName ? ConvertAddrToAnsiString(args->lpServiceStartName) : "NULL";

        stringStream << std::endl << "[+] CreateService Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        hSCManager: " << args->hSCManager << std::endl;
        stringStream << "        lpServiceName: " << serviceName << std::endl;
        stringStream << "        lpDisplayName: " << displayName << std::endl;
        stringStream << "        dwDesiredAccess: " << args->dwDesiredAccess << std::endl;
        stringStream << "        dwServiceType: " << args->dwServiceType << std::endl;
        stringStream << "        dwStartType: " << args->dwStartType << std::endl;
        stringStream << "        dwErrorControl: " << args->dwErrorControl << std::endl;
        stringStream << "        lpBinaryPathName: " << binaryPathName << std::endl;
        stringStream << "        lpLoadOrderGroup: " << args->lpLoadOrderGroup << std::endl;
        stringStream << "        lpdwTagId: " << args->lpdwTagId << std::endl;
        stringStream << "        lpDependencies: " << args->lpDependencies << std::endl;
        stringStream << "        lpServiceStartName: " << serviceStartName << std::endl;
        stringStream << "        lpPassword: " << args->lpPassword << std::endl;
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

VOID InstCreateService::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "CreateServiceA" || RTN_Name(rtn) == "CreateServiceW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hSCManager
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpServiceName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpDisplayName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // dwServiceType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwStartType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // dwErrorControl
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpBinaryPathName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpLoadOrderGroup
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpdwTagId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // lpDependencies
            IARG_FUNCARG_ENTRYPOINT_VALUE, 11, // lpServiceStartName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 12, // lpPassword
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hSCManager
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpServiceName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpDisplayName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // dwServiceType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwStartType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // dwErrorControl
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpBinaryPathName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpLoadOrderGroup
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpdwTagId
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // lpDependencies
            IARG_FUNCARG_ENTRYPOINT_VALUE, 11, // lpServiceStartName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 12, // lpPassword
            IARG_END);

        RTN_Close(rtn);
    }
}
