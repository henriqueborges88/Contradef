#include "InstNtCreateProcessEx.h"

std::map<CallContextKey, CallContext*> InstNtCreateProcessEx::callContextMap;
UINT32 InstNtCreateProcessEx::imgCallId = 0;
UINT32 InstNtCreateProcessEx::fcnCallId = 0;
Notifier* InstNtCreateProcessEx::globalNotifierPtr;

VOID InstNtCreateProcessEx::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtCreateProcessEx::callContextMap, InstNtCreateProcessEx::globalNotifierPtr);
}

VOID InstNtCreateProcessEx::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtCreateProcessEx::callContextMap, InstNtCreateProcessEx::globalNotifierPtr);
}

VOID InstNtCreateProcessEx::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtCreateProcessEx::callContextMap, InstNtCreateProcessEx::globalNotifierPtr);
}

VOID InstNtCreateProcessEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ParentProcess,
    ADDRINT InheritObjectTable, ADDRINT SectionHandle, ADDRINT DebugPort, ADDRINT ExceptionPort, ADDRINT CreateFlags) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtCreateProcessExArgs args;
    args.ProcessHandle = ProcessHandle;
    args.DesiredAccess = DesiredAccess;
    args.ObjectAttributes = ObjectAttributes;
    args.ParentProcess = ParentProcess;
    args.InheritObjectTable = InheritObjectTable;
    args.SectionHandle = SectionHandle;
    args.DebugPort = DebugPort;
    args.ExceptionPort = ExceptionPort;
    args.CreateFlags = CreateFlags;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtCreateProcessEx::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtCreateProcessEx::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtCreateProcessEx::globalNotifierPtr->Attach(instructionObserver);
    InstNtCreateProcessEx::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtCreateProcessEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ParentProcess,
    ADDRINT InheritObjectTable, ADDRINT SectionHandle, ADDRINT DebugPort, ADDRINT ExceptionPort, ADDRINT CreateFlags) {

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
        const InstNtCreateProcessExArgs* args = reinterpret_cast<InstNtCreateProcessExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        ProcessHandle: " << args->ProcessHandle << std::endl;
        stringStream << "        DesiredAccess: " << args->DesiredAccess << std::endl;
        stringStream << "        ObjectAttributes: " << args->ObjectAttributes << std::endl;
        stringStream << "        ParentProcess: " << args->ParentProcess << std::endl;
        stringStream << "        InheritObjectTable: " << args->InheritObjectTable << std::endl;
        stringStream << "        SectionHandle: " << args->SectionHandle << std::endl;
        stringStream << "        DebugPort: " << args->DebugPort << std::endl;
        stringStream << "        ExceptionPort: " << args->ExceptionPort << std::endl;
        stringStream << "        CreateFlags: " << args->CreateFlags << std::endl;
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

VOID InstNtCreateProcessEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtCreateProcessEx" || rtnName == "ZwCreateProcessEx") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ParentProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // InheritObjectTable
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // SectionHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // DebugPort
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ExceptionPort
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // CreateFlags
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ParentProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // InheritObjectTable
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // SectionHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // DebugPort
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ExceptionPort
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // CreateFlags
            IARG_END);

        RTN_Close(rtn);
    }
}

