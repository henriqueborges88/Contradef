#include "InstNtOpenKey.h"

std::map<CallContextKey, CallContext*> InstNtOpenKey::callContextMap;
UINT32 InstNtOpenKey::imgCallId = 0;
UINT32 InstNtOpenKey::fcnCallId = 0;
Notifier* InstNtOpenKey::globalNotifierPtr;

VOID InstNtOpenKey::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    NtOpenKeyArgs* args = new NtOpenKeyArgs;
    args->KeyHandle = KeyHandle; // OUT (PHANDLE)
    args->DesiredAccess = DesiredAccess;
    args->ObjectAttributes = ObjectAttributes;

    // Acessando a estrutura OBJECT_ATTRIBUTES
    OBJECT_ATTRIBUTES oa{};
    if (ObjectAttributes &&
        PIN_SafeCopy(&oa, (VOID*)ObjectAttributes, sizeof(oa)) == sizeof(oa) &&
        oa.ObjectName)
    {
        UNICODE_STRING us{};
        if (PIN_SafeCopy(&us, oa.ObjectName, sizeof(us)) == sizeof(us) &&
            us.Buffer && us.Length)
        {
            std::wstring w; w.resize(us.Length / sizeof(WCHAR));
            PIN_SafeCopy(&w[0], us.Buffer, us.Length);
            args->keyPathStr = WStringToString(w);
        }
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    // Registrar Parámetros
    std::stringstream& stringStream = callContext->stringStream;
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
    stringStream << "        keyPath: " << args->keyPathStr << std::endl;
    stringStream << "        DesiredAccess: 0x" << std::hex << DesiredAccess << std::dec << std::endl;
    stringStream << "        ObjectAttributes: 0x" << std::hex << ObjectAttributes << std::dec << std::endl;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    }

VOID InstNtOpenKey::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        
        std::stringstream& stringStream = callContext->stringStream;

        HANDLE hKey = 0;
        if (KeyHandle &&                       // o ponteiro não é nulo
            PIN_SafeCopy(&hKey,
                reinterpret_cast<const void*>(KeyHandle),
                sizeof(hKey)) == sizeof(hKey))
        {
            stringStream << "        KeyHandle: 0x" << std::hex << reinterpret_cast<UINT_PTR>(hKey) << std::dec << std::endl;
        }
        else
        {
            stringStream << "        KeyHandle: " << "Invalid or inaccessible pointer." << std::endl;
        }

        stringStream << "    Return Value (NTSTATUS): 0x" << std::hex << (NTSTATUS)retValue << std::dec << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<NtOpenKeyArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstNtOpenKey::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {
    
   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtOpenKey" || rtnName == "ZwOpenKey") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_END);
        RTN_Close(rtn);
    }

}