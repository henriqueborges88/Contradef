#include "InstShellExecute.h"

std::map<CallContextKey, CallContext*> InstShellExecute::callContextMap;
UINT32 InstShellExecute::imgCallId = 0;
UINT32 InstShellExecute::fcnCallId = 0;
Notifier* InstShellExecute::globalNotifierPtr;

VOID InstShellExecute::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstShellExecute::callContextMap, InstShellExecute::globalNotifierPtr);
}

VOID InstShellExecute::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstShellExecute::callContextMap, InstShellExecute::globalNotifierPtr);
}

VOID InstShellExecute::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstShellExecute::callContextMap, InstShellExecute::globalNotifierPtr);
}

VOID InstShellExecute::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hwnd, ADDRINT lpOperation, ADDRINT lpFile, ADDRINT lpParameters, ADDRINT lpDirectory, ADDRINT nShowCmd) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstShellExecuteArgs args;
    args.hwnd = hwnd;
    args.lpOperation = lpOperation;
    args.lpFile = lpFile;
    args.lpParameters = lpParameters;
    args.lpDirectory = lpDirectory;
    args.nShowCmd = nShowCmd;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstShellExecute::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstShellExecute::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstShellExecute::globalNotifierPtr->Attach(instructionObserver);
    InstShellExecute::globalNotifierPtr->Attach(traceObserver);
}

VOID InstShellExecute::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT hwnd, ADDRINT lpOperation, ADDRINT lpFile, ADDRINT lpParameters, ADDRINT lpDirectory, ADDRINT nShowCmd) {

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
        const InstShellExecuteArgs* args = reinterpret_cast<InstShellExecuteArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string operation = args->lpOperation ? ConvertAddrToAnsiString(args->lpOperation) : "NULL";
        std::string file = args->lpFile ? ConvertAddrToAnsiString(args->lpFile) : "NULL";
        std::string parameters = args->lpParameters ? ConvertAddrToAnsiString(args->lpParameters) : "NULL";
        std::string directory = args->lpDirectory ? ConvertAddrToAnsiString(args->lpDirectory) : "NULL";

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        hwnd: " << args->hwnd << std::endl;
        stringStream << "        lpOperation: " << operation << std::endl;
        stringStream << "        lpFile: " << file << std::endl;
        stringStream << "        lpParameters: " << parameters << std::endl;
        stringStream << "        lpDirectory: " << directory << std::endl;
        stringStream << "        nShowCmd: " << args->nShowCmd << std::endl;
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

VOID InstShellExecute::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (RTN_Name(rtn) == "ShellExecuteA" || RTN_Name(rtn) == "ShellExecuteW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpOperation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpParameters
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nShowCmd
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpOperation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpParameters
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nShowCmd
            IARG_END);

        RTN_Close(rtn);
    }
}
