#include "InstCreateFileW.h"

std::map<CallContextKey, CallContext*> InstCreateFileW::callContextMap;
UINT32 InstCreateFileW::imgCallId = 0;
UINT32 InstCreateFileW::fcnCallId = 0;
Notifier* InstCreateFileW::globalNotifierPtr;

VOID InstCreateFileW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpFileName, ADDRINT dwDesiredAccess, ADDRINT dwShareMode, ADDRINT lpSecurityAttributes, ADDRINT dwCreationDisposition, ADDRINT dwFlagsAndAttributes, ADDRINT hTemplateFile) {
    std::cout << "Chamando Create file" << std::endl;
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    CreateFileWArgs args;
    args.lpFileName = lpFileName;
    args.dwDesiredAccess = dwDesiredAccess;
    args.dwShareMode = dwShareMode;
    args.lpSecurityAttributes = lpSecurityAttributes;
    args.dwCreationDisposition = dwCreationDisposition;
    args.dwFlagsAndAttributes = dwFlagsAndAttributes;
    args.hTemplateFile = hTemplateFile;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, &args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;
}

VOID InstCreateFileW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT lpFileName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        const CreateFileWArgs* args = reinterpret_cast<CreateFileWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wsFileName = ConvertAddrToWideString(args->lpFileName);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);

        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parâmetros: " << std::endl;
        stringStream << "        wsFileName: " << WStringToString(wsFileName) << std::endl;
        stringStream << "        dwDesiredAccess: " << args->dwDesiredAccess << std::endl;
        stringStream << "        dwShareMode: " << args->dwShareMode << std::endl;
        stringStream << "        lpSecurityAttributes: " << args->lpSecurityAttributes << std::endl;
        stringStream << "        dwCreationDisposition: " << args->dwCreationDisposition << std::endl;
        stringStream << "        dwFlagsAndAttributes: " << args->dwFlagsAndAttributes << std::endl;
        stringStream << "        hTemplateFile: " << args->hTemplateFile << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;
        stringStream << "[*] Concluído" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstCreateFileW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateFileW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwShareMode
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpSecurityAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // dwCreationDisposition
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwFlagsAndAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // hTemplateFile
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpFileName
            IARG_END);

        RTN_Close(rtn);
    }
}
