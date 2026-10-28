#include "InstRegOpenKeyW.h"

CallContextMap InstRegOpenKeyW::callContextMap;
UINT32 InstRegOpenKeyW::imgCallId = 0;
CallSequenceNumber InstRegOpenKeyW::fcnCallId = 0;
Notifier* InstRegOpenKeyW::globalNotifierPtr;


VOID InstRegOpenKeyW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegOpenKeyWArgs* args = new InstRegOpenKeyWArgs;
    args->hKey = hKey;
    args->lpSubKey = lpSubKey;
    args->phkResult = phkResult;

    //// CONTRAMEDIDA
    //args->originalLastChar = *reinterpret_cast<wchar_t*>(lpSubKey + wcslen(reinterpret_cast<wchar_t*>(lpSubKey)) - 1); // Salvar o �ltimo caractere original de lpSubKey

    //std::wstring wslpSubKey = ConvertAddrToWideString(lpSubKey);
    //std::string ansiStringLpSubKey = WStringToString(wslpSubKey);
    //if (isRegistryKeyPartInList(ansiStringLpSubKey)) {
    //    wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(lpSubKey);
    //    size_t len = wcslen(wideCharStr);

    //    if (len > 0) {
    //        wideCharStr[len - 1] = L'q';
    //        args->isModified = true;
    //    }
    //}
    ////

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstRegOpenKeyW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = pendingCallContext;
        // Registrar Parámetros
        const InstRegOpenKeyWArgs* args = reinterpret_cast<InstRegOpenKeyWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringLpSubKey = ConvertAddrToWideString(args->lpSubKey);

        ////CONTRAMEDIDA (FALHANDO)
        //if (args->isModified) {
        //    wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(lpSubKey);
        //    size_t len = wcslen(wideCharStr);

        //    if (len > 0) {
        //        wideCharStr[len - 1] = args->originalLastChar;
        //    }
        //}
        ////
        HANDLE* phk = reinterpret_cast<HANDLE*>(args->phkResult);
        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hKey: " << std::hex << (HANDLE)args->hKey << std::dec << std::endl;
        stringStream << "        lpSubKey: " << WStringToString(wideStringLpSubKey) << std::endl;
        stringStream << "        phkResult: " << args->phkResult << " (" << std::hex << *phk << std::dec << ")" << std::endl;
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

VOID InstRegOpenKeyW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegOpenKeyW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }
}
