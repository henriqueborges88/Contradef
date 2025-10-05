#include "InstBaseRegOpenKey.h"

std::map<CallContextKey, CallContext*> InstBaseRegOpenKey::callContextMap;
UINT32 InstBaseRegOpenKey::imgCallId = 0;
UINT32 InstBaseRegOpenKey::fcnCallId = 0;
Notifier* InstBaseRegOpenKey::globalNotifierPtr;


VOID InstBaseRegOpenKey::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    BaseRegOpenKeyArgs* args = new BaseRegOpenKeyArgs;
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

    
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    }

VOID InstBaseRegOpenKey::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retVal, ADDRINT* returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = it->second;
        // Registrar Parámetros
        const BaseRegOpenKeyArgs* args = reinterpret_cast<BaseRegOpenKeyArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wslpSubKey = ConvertAddrToWideString(args->lpSubKey);

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
        
        stringStream << std::endl << "[+] BaseRegOpen..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hKey: " << std::hex << (HANDLE)args->hKey << std::dec << std::endl;
        stringStream << "        lpSubKey: " << WStringToString(wslpSubKey) << std::endl;
        stringStream << "        phkResult: " << args->phkResult << " (" << std::hex << *phk << std::dec << ")" << std::endl;
        stringStream << "    Return Value: " << retVal << std::endl;
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

VOID InstBaseRegOpenKey::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "BaseRegOpenKey") {
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