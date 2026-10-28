#include "InstLoadLibraryEx.h"

CallContextMap InstLoadLibraryEx::callContextMap;
UINT32 InstLoadLibraryEx::imgCallId = 0;
CallSequenceNumber InstLoadLibraryEx::fcnCallId = 0;
Notifier* InstLoadLibraryEx::globalNotifierPtr;
BOOL InstLoadLibraryEx::wideVersion = false;

VOID InstLoadLibraryEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpLibFileName, ADDRINT hFile, ADDRINT dwFlags) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstLoadLibraryExArgs* args = new InstLoadLibraryExArgs;
    args->lpLibFileName = lpLibFileName;
    args->hFile = hFile;
    args->dwFlags = dwFlags;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;

    std::string libFileName;
    if (wideVersion) {
        std::wstring libFileNameW;
        libFileNameW = lpLibFileName ? ConvertAddrToWideString(lpLibFileName) : L"NULL";
        libFileName = WStringToString(libFileNameW);
    }
    else {
        libFileName = lpLibFileName ? ConvertAddrToAnsiString(lpLibFileName) : "NULL";
    }
    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    stringStream << "    Module name: " << IMG_Name(img) << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpLibFileName: " << libFileName << std::endl;
    stringStream << "        hFile: " << hFile << std::endl;
    stringStream << "        dwFlags: " << dwFlags << std::endl;

    }

VOID InstLoadLibraryEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT lpLibFileName, ADDRINT hFile, ADDRINT dwFlags) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {

        CallContext* callContext = pendingCallContext;
        // Registrar Parámetros
        const InstLoadLibraryExArgs* args = reinterpret_cast<InstLoadLibraryExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstLoadLibraryEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "LoadLibraryEx" || rtnName == "LoadLibraryExA" || rtnName == "LoadLibraryExW") {
        if (rtnName == "LoadLibraryExW") {
            wideVersion = true;
        }

        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLibFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFlags
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLibFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFlags
            IARG_END);

        RTN_Close(rtn);
    }
}
