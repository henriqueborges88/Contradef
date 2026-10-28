#include "InstGetProcAddress.h"

CallContextMap InstGetProcAddress::callContextMap;
UINT32 InstGetProcAddress::imgCallId = 0;
CallSequenceNumber InstGetProcAddress::fcnCallId = 0;
Notifier* InstGetProcAddress::globalNotifierPtr;

VOID InstGetProcAddress::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hModule, ADDRINT lpProcName) {
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    GetProcAddressArgs* args = new GetProcAddressArgs;
    args->hModule = hModule;
    args->lpProcName = lpProcName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetProcAddress..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hModule: 0x" << std::hex << hModule << std::dec << std::endl;

    // Recuperar o nome da função (lpProcName)
    std::string procName;
    if (lpProcName != 0) {
        CHAR buffer[256] = { 0 };
        PIN_SafeCopy(buffer, reinterpret_cast<CHAR*>(lpProcName), sizeof(buffer) - 1);
        procName = buffer;
    }

    stringStream << "        lpProcName: 0x" << std::hex << lpProcName << std::dec << " (" << procName << ")" << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetProcAddress" << std::endl;

}

VOID InstGetProcAddress::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT hModule, ADDRINT lpProcName) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        {
            using namespace WindowsAPI;
            FARPROC result = reinterpret_cast<FARPROC>(retValue);
            stringStream << "    Returned from GetProcAddress: 0x" << std::hex << retValue << std::dec << std::endl;

            if (result != NULL) {
                stringStream << "    Function obtained successfully." << std::endl;
            }
            else {
                DWORD error = GetLastError();
                stringStream << "    Failed to get function address. Error code: " << error << std::endl;
            }
        }

        stringStream << "  [-] Call from GetProcAddress completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetProcAddressArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetProcAddress::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetProcAddress") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura de GetProcAddress:
        // FARPROC GetProcAddress(
        //   HMODULE hModule,
        //   LPCSTR  lpProcName
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpProcName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE, // Valor de retorno (FARPROC)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hModule
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpProcName
            IARG_END);

        RTN_Close(rtn);
    }
}
