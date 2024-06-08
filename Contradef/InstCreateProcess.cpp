#include "InstCreateProcess.h"

std::map<CallContextKey, CallContext*> InstCreateProcess::callContextMap;
UINT32 InstCreateProcess::imgCallId = 0;
UINT32 InstCreateProcess::fcnCallId = 0;
Notifier* InstCreateProcess::globalNotifierPtr;

VOID InstCreateProcess::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstCreateProcess::callContextMap, InstCreateProcess::globalNotifierPtr);
}

VOID InstCreateProcess::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstCreateProcess::callContextMap, InstCreateProcess::globalNotifierPtr);
}

VOID InstCreateProcess::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstCreateProcess::callContextMap, InstCreateProcess::globalNotifierPtr);
}

VOID InstCreateProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpApplicationName, ADDRINT lpCommandLine, ADDRINT lpProcessAttributes, ADDRINT lpThreadAttributes,
    ADDRINT bInheritHandles, ADDRINT dwCreationFlags, ADDRINT lpEnvironment, ADDRINT lpCurrentDirectory,
    ADDRINT lpStartupInfo, ADDRINT lpProcessInformation) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstCreateProcessArgs args;
    args.lpApplicationName = lpApplicationName;
    args.lpCommandLine = lpCommandLine;
    args.lpProcessAttributes = lpProcessAttributes;
    args.lpThreadAttributes = lpThreadAttributes;
    args.bInheritHandles = bInheritHandles;
    args.dwCreationFlags = dwCreationFlags;
    args.lpEnvironment = lpEnvironment;
    args.lpCurrentDirectory = lpCurrentDirectory;
    args.lpStartupInfo = lpStartupInfo;
    args.lpProcessInformation = lpProcessInformation;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstCreateProcess::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstCreateProcess::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstCreateProcess::globalNotifierPtr->Attach(instructionObserver);
    InstCreateProcess::globalNotifierPtr->Attach(traceObserver);
}

VOID InstCreateProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT lpApplicationName, ADDRINT lpCommandLine, ADDRINT lpProcessAttributes, ADDRINT lpThreadAttributes,
    ADDRINT bInheritHandles, ADDRINT dwCreationFlags, ADDRINT lpEnvironment, ADDRINT lpCurrentDirectory,
    ADDRINT lpStartupInfo, ADDRINT lpProcessInformation) {

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
        const InstCreateProcessArgs* args = reinterpret_cast<InstCreateProcessArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string applicationName = args->lpApplicationName ? ConvertAddrToAnsiString(args->lpApplicationName) : "NULL";
        std::string commandLine = args->lpCommandLine ? ConvertAddrToAnsiString(args->lpCommandLine) : "NULL";

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        lpApplicationName: " << applicationName << std::endl;
        stringStream << "        lpCommandLine: " << commandLine << std::endl;
        stringStream << "        lpProcessAttributes: " << args->lpProcessAttributes << std::endl;
        stringStream << "        lpThreadAttributes: " << args->lpThreadAttributes << std::endl;
        stringStream << "        bInheritHandles: " << args->bInheritHandles << std::endl;
        stringStream << "        dwCreationFlags: " << args->dwCreationFlags << std::endl;
        stringStream << "        lpEnvironment: " << args->lpEnvironment << std::endl;
        stringStream << "        lpCurrentDirectory: " << args->lpCurrentDirectory << std::endl;
        stringStream << "        lpStartupInfo: " << args->lpStartupInfo << std::endl;
        stringStream << "        lpProcessInformation: " << args->lpProcessInformation << std::endl;
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

VOID InstCreateProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateProcessA" || rtnName == "CreateProcessW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpApplicationName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpCommandLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpProcessAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpThreadAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // bInheritHandles
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwCreationFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpEnvironment
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpCurrentDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpStartupInfo
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpProcessInformation
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpApplicationName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpCommandLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpProcessAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpThreadAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // bInheritHandles
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwCreationFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpEnvironment
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpCurrentDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpStartupInfo
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpProcessInformation
            IARG_END);

        RTN_Close(rtn);
    }
}

