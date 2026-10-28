#include "InstCreateToolhelp32Snapshot.h"

CallContextMap InstCreateToolhelp32Snapshot::callContextMap;
UINT32 InstCreateToolhelp32Snapshot::imgCallId = 0;
CallSequenceNumber InstCreateToolhelp32Snapshot::fcnCallId = 0;
Notifier* InstCreateToolhelp32Snapshot::globalNotifierPtr;

VOID InstCreateToolhelp32Snapshot::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT dwFlags, ADDRINT th32ProcessID) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    CreateToolhelp32SnapshotArgs* args = new CreateToolhelp32SnapshotArgs;
    args->dwFlags = dwFlags;
    args->th32ProcessID = th32ProcessID;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] CreateToolhelp32Snapshot..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        dwFlags: 0x" << std::hex << dwFlags << std::dec << std::endl;
    stringStream << "        th32ProcessID: " << th32ProcessID << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call CreateToolhelp32Snapshot" << std::endl;

}

VOID InstCreateToolhelp32Snapshot::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT dwFlags, ADDRINT th32ProcessID) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        HANDLE result = reinterpret_cast<HANDLE>(retValue);
        stringStream << "    Returned from CreateToolhelp32Snapshot: 0x" << std::hex << retValue << std::dec << std::endl;

        if (result != (HANDLE)-1) { // INVALID_HANDLE_VALUE -> -1
            stringStream << "    Snapshot created successfully." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to create snapshot. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from CreateToolhelp32Snapshot completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<CreateToolhelp32SnapshotArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstCreateToolhelp32Snapshot::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateToolhelp32Snapshot") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // HANDLE CreateToolhelp32Snapshot(
        //   DWORD dwFlags,
        //   DWORD th32ProcessID
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // th32ProcessID
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (HANDLE)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // dwFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // th32ProcessID
            IARG_END);

        RTN_Close(rtn);
    }
}
