#include "InstRegEnumKeyW.h"

CallContextMap InstRegEnumKeyW::callContextMap;
UINT32 InstRegEnumKeyW::imgCallId = 0;
CallSequenceNumber InstRegEnumKeyW::fcnCallId = 0;
Notifier* InstRegEnumKeyW::globalNotifierPtr;


VOID InstRegEnumKeyW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegEnumKeyWArgs* args = new InstRegEnumKeyWArgs;
    args->hKey = hKey;
    args->dwIndex = dwIndex;
    args->lpName = lpName;
    args->lpcName = lpcName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstRegEnumKeyW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcName) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
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
        const InstRegEnumKeyWArgs* args = reinterpret_cast<InstRegEnumKeyWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::wstring wideStringLpName = ConvertAddrToWideString(args->lpName);
        std::string ansiStringLpName(wideStringLpName.begin(), wideStringLpName.end());

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hKey: " << std::hex << (HANDLE)args->hKey << std::dec << std::endl;
        stringStream << "        dwIndex: " << args->dwIndex << std::endl;
        stringStream << "        lpName: " << ansiStringLpName << std::endl;
        stringStream << "        lpcName: " << args->lpcName << std::endl;
        stringStream << "    Return Value: " << retValue << std::endl;
        if (isRegistryKeyPartInList(ansiStringLpName)) {
            //std::cout << "A chave de registro est� na lista." << std::endl;
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //retValue = 0;
            //alterar o valor de retorno
            //verificar a necessidade de chamar RegCloseKey para encerrar o handle
            //se poss�vel, alterar o valor da chave de registro antes da chamada, colocar uma chave que n�o existe
        }
        else {
            //std::cout << "A chave de registro n�o est� na lista." << std::endl;
        }

        delete args;
        delete callContext;
    }

    fcnCallId++;

}

VOID InstRegEnumKeyW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegEnumKeyW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcName
            IARG_END);

        RTN_Close(rtn);
    }
}
