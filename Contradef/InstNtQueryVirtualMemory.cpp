#include "InstNtQueryVirtualMemory.h"

std::map<CallContextKey, CallContext*> InstNtQueryVirtualMemory::callContextMap;
UINT32 InstNtQueryVirtualMemory::imgCallId = 0;
UINT32 InstNtQueryVirtualMemory::fcnCallId = 0;
Notifier* InstNtQueryVirtualMemory::globalNotifierPtr;

VOID InstNtQueryVirtualMemory::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtQueryVirtualMemory::callContextMap, InstNtQueryVirtualMemory::globalNotifierPtr);
}

VOID InstNtQueryVirtualMemory::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtQueryVirtualMemory::callContextMap, InstNtQueryVirtualMemory::globalNotifierPtr);
}

VOID InstNtQueryVirtualMemory::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtQueryVirtualMemory::callContextMap, InstNtQueryVirtualMemory::globalNotifierPtr);
}

VOID InstNtQueryVirtualMemory::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT BaseAddress, ADDRINT MemoryInformationClass, ADDRINT MemoryInformation,
    ADDRINT MemoryInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtQueryVirtualMemoryArgs args;
    args.ProcessHandle = ProcessHandle;
    args.BaseAddress = BaseAddress;
    args.MemoryInformationClass = MemoryInformationClass;
    args.MemoryInformation = MemoryInformation;
    args.MemoryInformationLength = MemoryInformationLength;
    args.ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtQueryVirtualMemory::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtQueryVirtualMemory::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtQueryVirtualMemory::globalNotifierPtr->Attach(instructionObserver);
    InstNtQueryVirtualMemory::globalNotifierPtr->Attach(traceObserver);
}

VOID InstNtQueryVirtualMemory::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT ProcessHandle, ADDRINT BaseAddress, ADDRINT MemoryInformationClass, ADDRINT MemoryInformation,
    ADDRINT MemoryInformationLength, ADDRINT ReturnLength) {

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
        const InstNtQueryVirtualMemoryArgs* args = reinterpret_cast<InstNtQueryVirtualMemoryArgs*>(callContext->functionArgs);
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
        stringStream << "        BaseAddress: " << args->BaseAddress << std::endl;
        stringStream << "        MemoryInformationClass: " << args->MemoryInformationClass << std::endl;
        stringStream << "        MemoryInformation: " << args->MemoryInformation << std::endl;
        stringStream << "        MemoryInformationLength: " << args->MemoryInformationLength << std::endl;
        stringStream << "        ReturnLength: " << args->ReturnLength << std::endl;
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

VOID InstNtQueryVirtualMemory::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtQueryVirtualMemory" || rtnName == "ZwQueryVirtualMemory") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // BaseAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // MemoryInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // MemoryInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // MemoryInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // ReturnLength
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // BaseAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // MemoryInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // MemoryInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // MemoryInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // ReturnLength
            IARG_END);

        RTN_Close(rtn);
    }
}