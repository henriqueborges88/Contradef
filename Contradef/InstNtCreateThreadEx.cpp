#include "InstNtCreateThreadEx.h"

std::map<CallContextKey, CallContext*> InstNtCreateThreadEx::callContextMap;
UINT32 InstNtCreateThreadEx::imgCallId = 0;
UINT32 InstNtCreateThreadEx::fcnCallId = 0;
Notifier* InstNtCreateThreadEx::globalNotifierPtr;

VOID InstNtCreateThreadEx::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtCreateThreadEx::callContextMap, InstNtCreateThreadEx::globalNotifierPtr);
}

VOID InstNtCreateThreadEx::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtCreateThreadEx::callContextMap, InstNtCreateThreadEx::globalNotifierPtr);
}

VOID InstNtCreateThreadEx::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtCreateThreadEx::callContextMap, InstNtCreateThreadEx::globalNotifierPtr);
}

VOID InstNtCreateThreadEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ProcessHandle,
    ADDRINT StartAddress, ADDRINT Parameter, ADDRINT CreateFlags, ADDRINT ZeroBits,
    ADDRINT StackSize, ADDRINT MaximumStackSize, ADDRINT AttributeList) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtCreateThreadExArgs args;
    args.ThreadHandle = ThreadHandle;
    args.DesiredAccess = DesiredAccess;
    args.ObjectAttributes = ObjectAttributes;
    args.ProcessHandle = ProcessHandle;
    args.StartAddress = StartAddress;
    args.Parameter = Parameter;
    args.CreateFlags = CreateFlags;
    args.ZeroBits = ZeroBits;
    args.StackSize = StackSize;
    args.MaximumStackSize = MaximumStackSize;
    args.AttributeList = AttributeList;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtCreateThreadEx::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtCreateThreadEx::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtCreateThreadEx::globalNotifierPtr->Attach(instructionObserver);
    InstNtCreateThreadEx::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtCreateThreadEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ProcessHandle,
    ADDRINT StartAddress, ADDRINT Parameter, ADDRINT CreateFlags, ADDRINT ZeroBits,
    ADDRINT StackSize, ADDRINT MaximumStackSize, ADDRINT AttributeList) {

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
        const InstNtCreateThreadExArgs* args = reinterpret_cast<InstNtCreateThreadExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << std::endl << "[+] NtCreateThreadEx Detected..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        ThreadHandle: " << args->ThreadHandle << std::endl;
        stringStream << "        DesiredAccess: " << args->DesiredAccess << std::endl;
        stringStream << "        ObjectAttributes: " << args->ObjectAttributes << std::endl;
        stringStream << "        ProcessHandle: " << args->ProcessHandle << std::endl;
        stringStream << "        StartAddress: " << args->StartAddress << std::endl;
        stringStream << "        Parameter: " << args->Parameter << std::endl;
        stringStream << "        CreateFlags: " << args->CreateFlags << std::endl;
        stringStream << "        ZeroBits: " << args->ZeroBits << std::endl;
        stringStream << "        StackSize: " << args->StackSize << std::endl;
        stringStream << "        MaximumStackSize: " << args->MaximumStackSize << std::endl;
        stringStream << "        AttributeList: " << args->AttributeList << std::endl;
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

VOID InstNtCreateThreadEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtCreateThreadEx") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // StartAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // Parameter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // CreateFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ZeroBits
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // StackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // MaximumStackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // AttributeList
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // StartAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // Parameter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // CreateFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ZeroBits
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // StackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // MaximumStackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // AttributeList
            IARG_END);

        RTN_Close(rtn);
    }
}
