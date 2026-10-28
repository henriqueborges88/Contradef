#include "InstGetModuleFileNameExW.h"

CallContextMap InstGetModuleFileNameExW::callContextMap;
UINT32 InstGetModuleFileNameExW::imgCallId = 0;
CallSequenceNumber InstGetModuleFileNameExW::fcnCallId = 0;
Notifier* InstGetModuleFileNameExW::globalNotifierPtr;


VOID InstGetModuleFileNameExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    GetModuleFileNameExWArgs* args = new GetModuleFileNameExWArgs;
    args->hProcess = hProcess;
    args->hModule = hModule;
    args->lpFilename = lpFilename;
    args->nSize = nSize;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    std::stringstream& stringStream = callContext->stringStream;
    std::wstring wideStringLpFilename = ConvertAddrToWideString(lpFilename);
    
    stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    stringStream << "    Module name: " << IMG_Name(img) << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hProcess: " << hProcess << std::endl;
    stringStream << "        hModule: " << hModule << std::endl;
    stringStream << "        lpFilename: " << WStringToString(wideStringLpFilename) << std::endl;
    stringStream << "        nSize: " << nSize << std::endl;

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstGetModuleFileNameExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetModuleFileNameExWArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetModuleFileNameExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    //SEC sec = RTN_Sec(rtn);
    //IMG img = SEC_Img(sec);
    //if (IMG_Valid(img)) {
    //    std::string imageName = IMG_Name(img);
    //    std::string moduleName = ExtractModuleName(IMG_Name(img));
    //    if (toUpperCase(moduleName) != "KERNELBASE.DLL") {
    //        return;
    //    }
    //}
    //else {
    //    return;
    //}

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetModuleFileNameExW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFilename
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // nSize
            IARG_END);

        RTN_Close(rtn);
    }
}

