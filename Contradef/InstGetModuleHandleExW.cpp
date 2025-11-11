#include "InstGetModuleHandleExW.h"

std::map<CallContextKey, CallContext*> InstGetModuleHandleExW::callContextMap;
UINT32 InstGetModuleHandleExW::imgCallId = 0;
UINT32 InstGetModuleHandleExW::fcnCallId = 0;
Notifier* InstGetModuleHandleExW::globalNotifierPtr = nullptr;

VOID InstGetModuleHandleExW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT returnAddress,
    ADDRINT dwFlags, ADDRINT lpModuleName, ADDRINT phModule)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    auto* args = new GetModuleHandleExWArgs;
    args->dwFlags = dwFlags;
    args->lpModuleName = lpModuleName;
    args->phModule = phModule;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;
}

VOID InstGetModuleHandleExW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT* retValueRef,
    ADDRINT* returnAddress,
    ADDRINT dwFlags, ADDRINT lpModuleName, ADDRINT phModule)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {

        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = it->second;
        const GetModuleHandleExWArgs* args = reinterpret_cast<GetModuleHandleExWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::wstring wName = ConvertAddrToWideString(args->lpModuleName);

        // Tenta ler o HMODULE apontado por phModule (seguindo o seu padrão de dereferenciar ponteiros)
        WindowsAPI::HMODULE* pHM = reinterpret_cast<WindowsAPI::HMODULE*>(args->phModule);
        ADDRINT hmValue = 0;
        if (pHM) {
            // Opcionalmente, poderia usar PIN_SafeCopy. Aqui seguimos seu padrão direto:
            hmValue = (ADDRINT)(*pHM);
        }

        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;

        stringStream << "    Parameters: " << std::endl;
        stringStream << "        dwFlags: 0x" << std::hex << static_cast<DWORD>(args->dwFlags) << std::dec << std::endl;
        stringStream << "        lpModuleName: " << WStringToString(wName) << std::endl;
        stringStream << "        phModule: " << std::hex << (ADDRINT)args->phModule << std::dec
            << " (value: 0x" << std::hex << hmValue << std::dec << ")" << std::endl;
        INT retValue = *(INT*)retValueRef;
        stringStream << "    Return Value: " << (retValue ? "TRUE" : "FALSE") << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstGetModuleHandleExW::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetModuleHandleExW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // BOOL GetModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE* phModule);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                    // endereço do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  // dwFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  // lpModuleName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,  // phModule
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_REFERENCE,      // REF retorno
            IARG_REG_VALUE, REG_STACK_PTR,     // endereço de retorno (caller) via stack ptr
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  // dwFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  // lpModuleName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,  // phModule
            IARG_END);

        RTN_Close(rtn);
    }
}
