#include "InstLoadLibraryEx.h"

std::map<CallContextKey, CallContext*> InstLoadLibraryEx::callContextMap;
UINT32 InstLoadLibraryEx::imgCallId = 0;
UINT32 InstLoadLibraryEx::fcnCallId = 0;
Notifier* InstLoadLibraryEx::globalNotifierPtr;

VOID InstLoadLibraryEx::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstLoadLibraryEx::callContextMap, InstLoadLibraryEx::globalNotifierPtr);
}

VOID InstLoadLibraryEx::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstLoadLibraryEx::callContextMap, InstLoadLibraryEx::globalNotifierPtr);
}

VOID InstLoadLibraryEx::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstLoadLibraryEx::callContextMap, InstLoadLibraryEx::globalNotifierPtr);
}

VOID InstLoadLibraryEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpLibFileName, ADDRINT hFile, ADDRINT dwFlags) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstLoadLibraryExArgs args;
    args.lpLibFileName = lpLibFileName;
    args.hFile = hFile;
    args.dwFlags = dwFlags;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstLoadLibraryEx::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstLoadLibraryEx::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstLoadLibraryEx::globalNotifierPtr->Attach(instructionObserver);
    InstLoadLibraryEx::globalNotifierPtr->Attach(traceObserver);
}

VOID InstLoadLibraryEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
    ADDRINT lpLibFileName, ADDRINT hFile, ADDRINT dwFlags) {

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
        const InstLoadLibraryExArgs* args = reinterpret_cast<InstLoadLibraryExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string libFileName = args->lpLibFileName ? ConvertAddrToAnsiString(args->lpLibFileName) : "NULL";

        // Obter a RTN da instru��o atual
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do m�dulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endere�o da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Par�metros: " << std::endl;
        stringStream << "        lpLibFileName: " << libFileName << std::endl;
        stringStream << "        hFile: " << args->hFile << std::endl;
        stringStream << "        dwFlags: " << args->dwFlags << std::endl;
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

VOID InstLoadLibraryEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "LoadLibraryExA" || rtnName == "LoadLibraryExW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLibFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFlags
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLibFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFlags
            IARG_END);

        RTN_Close(rtn);
    }
}
