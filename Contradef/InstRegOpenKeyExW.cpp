#include "InstRegOpenKeyExW.h"

std::map<CallContextKey, CallContext*> InstRegOpenKeyExW::callContextMap;
UINT32 InstRegOpenKeyExW::imgCallId = 0;
UINT32 InstRegOpenKeyExW::fcnCallId = 0;
Notifier* InstRegOpenKeyExW::globalNotifierPtr;


VOID InstRegOpenKeyExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    RegOpenKeyExWArgs* args = new RegOpenKeyExWArgs;
    args->hKey = hKey;
    args->lpSubKey = lpSubKey;
    args->ulOptions = ulOptions;
    args->samDesired = samDesired;
    args->phkResult = phkResult;

    //// CONTRAMEDIDA (FALHANDO) VERIFICAR
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

VOID InstRegOpenKeyExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {

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
        const RegOpenKeyExWArgs* args = reinterpret_cast<RegOpenKeyExWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringLpSubKey = ConvertAddrToWideString(args->lpSubKey);

        ////CONTRAMEDIDA REVERTENDO (FALAHNDO)
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
        stringStream << "        ulOptions: " << args->ulOptions << std::endl;
        stringStream << "        samDesired: " << args->samDesired << std::endl;
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

VOID InstRegOpenKeyExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

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
    if (rtnName == "RegOpenKeyExW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }
}