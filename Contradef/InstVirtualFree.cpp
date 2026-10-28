#include "InstVirtualFree.h"

CallContextMap InstVirtualFree::callContextMap;
UINT32 InstVirtualFree::imgCallId = 0;
CallSequenceNumber InstVirtualFree::fcnCallId = 0;
Notifier* InstVirtualFree::globalNotifierPtr;

VOID InstVirtualFree::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpAddress, ADDRINT dwSize, ADDRINT dwFreeType) {
    PIN_LockClient();
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    VirtualFreeArgs* args = new VirtualFreeArgs;
    args->lpAddress = lpAddress;
    args->dwSize = dwSize;
    args->dwFreeType = dwFreeType;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] VirtualFree..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpAddress: 0x" << std::hex << lpAddress << std::dec << std::endl;
    stringStream << "        dwSize: " << dwSize << " bytes" << std::endl;
    stringStream << "        dwFreeType: 0x" << std::hex << dwFreeType << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call VirtualFree" << std::endl;

}

VOID InstVirtualFree::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT lpAddress, ADDRINT dwSize, ADDRINT dwFreeType) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from VirtualFree: " << result << std::endl;

        if (result != 0) {
            stringStream << "    Successful memory release." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Memory release failure. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from VirtualFree completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const VirtualFreeArgs* args = reinterpret_cast<VirtualFreeArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstVirtualFree::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "VirtualFree") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFreeType
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (BOOL)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwFreeType
            IARG_END);

        RTN_Close(rtn);
    }
}
