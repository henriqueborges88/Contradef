#include "InstRegEnumKeyExW.h"

std::map<CallContextKey, CallContext*> InstRegEnumKeyExW::callContextMap;
UINT32 InstRegEnumKeyExW::imgCallId = 0;
UINT32 InstRegEnumKeyExW::fcnCallId = 0;
Notifier* InstRegEnumKeyExW::globalNotifierPtr;

VOID InstRegEnumKeyExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcchName, ADDRINT lpReserved,
    ADDRINT lpClass, ADDRINT lpcchClass, ADDRINT lpftLastWriteTime) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RegEnumKeyExWArgs* args = new RegEnumKeyExWArgs;
    args->hKey = hKey;
    args->dwIndex = dwIndex;
    args->lpName = lpName;
    args->lpcchName = lpcchName;
    args->lpReserved = lpReserved;
    args->lpClass = lpClass;
    args->lpcchClass = lpcchClass;
    args->lpftLastWriteTime = lpftLastWriteTime;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] RegEnumKeyExW..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hKey: 0x" << std::hex << (HANDLE)hKey << std::dec << std::endl;
    stringStream << "        dwIndex: " << dwIndex << std::endl;
    stringStream << "        lpName: 0x" << std::hex << lpName << std::dec << ", lpcchName: 0x" << std::hex << lpcchName << std::dec << std::endl;
    stringStream << "        lpReserved: 0x" << std::hex << lpReserved << std::dec << std::endl;
    stringStream << "        lpClass: 0x" << std::hex << lpClass << std::dec << ", lpcchClass: 0x" << std::hex << lpcchClass << std::dec << std::endl;
    stringStream << "        lpftLastWriteTime: 0x" << std::hex << lpftLastWriteTime << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call RegEnumKeyExW" << std::endl;

}

VOID InstRegEnumKeyExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT hKey, ADDRINT dwIndex, ADDRINT lpName, ADDRINT lpcchName,
    ADDRINT lpReserved, ADDRINT lpClass, ADDRINT lpcchClass, ADDRINT lpftLastWriteTime) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        LONG result = static_cast<LONG>(retValue);
        stringStream << "    Returned from RegEnumKeyExW: " << result << std::endl;

        if (result == ERROR_SUCCESS) {
            // Sucesso: tenta ler o nome e a classe da subchave

            stringStream << "    Subkey name: " << WStringToString(ConvertAddrToWideString(lpcchName)) << std::endl;
            stringStream << "    Subkey class: " << WStringToString(ConvertAddrToWideString(lpcchClass)) << std::endl;

            // Opcional: poderia ler lpftLastWriteTime se desejado, mas muitas vezes não é necessário.

            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            // Falha
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Operation failed. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from RegEnumKeyExW completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const RegEnumKeyExWArgs* args = reinterpret_cast<RegEnumKeyExWArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstRegEnumKeyExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegEnumKeyExW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // LSTATUS RegEnumKeyExW(
        //   HKEY      hKey,
        //   DWORD     dwIndex,
        //   LPWSTR    lpName,
        //   LPDWORD   lpcchName,
        //   LPDWORD   lpReserved,
        //   LPWSTR    lpClass,
        //   LPDWORD   lpcchClass,
        //   PFILETIME lpftLastWriteTime
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcchName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpcchClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpftLastWriteTime
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // LSTATUS retorno
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwIndex
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpcchName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpcchClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpftLastWriteTime
            IARG_END);

        RTN_Close(rtn);
    }
}
