#include "InstRtlAllocateHeap.h"

CallContextMap InstRtlAllocateHeap::callContextMap;
UINT32 InstRtlAllocateHeap::imgCallId = 0;
CallSequenceNumber InstRtlAllocateHeap::fcnCallId = 0;
Notifier* InstRtlAllocateHeap::globalNotifierPtr = nullptr;

VOID InstRtlAllocateHeap::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*, ADDRINT retIP, ADDRINT HeapHandle, ADDRINT Flags, ADDRINT Size)
{
    if (instrumentOnlyMain && !IsMainExecutable(retIP)) return;

    auto* args = new RtlAllocateHeapArgs{ HeapHandle, Flags, Size };

    UINT32 ctxId = callId * 100 + fcnCallId;
    auto* cc = new CallContext(ctxId, tid, inst, args);
    SaveCallContext(callContextMap, {callId, tid}, cc);

    std::stringstream& ss = cc->stringStream;
    ss << "\n[+] RtlAllocateHeap...\n"
        << "    Thread: " << tid << '\n'
        << "    Call Id: " << fcnCallId << '\n'
        << "    Routine address: 0x" << std::hex << cc->rtnAddress << std::dec << '\n'
        << "    Parameters:\n"
        << "        HeapHandle: 0x" << std::hex << HeapHandle << std::dec << '\n'
        << "        Flags: 0x" << std::hex << Flags << std::dec << '\n'
        << "        Size: " << Size << " bytes\n"
        << "    Address of calling function: 0x" << std::hex << retIP << std::dec << '\n'
        << "  [-] Start of call RtlAllocateHeap\n";

}

VOID InstRtlAllocateHeap::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
    ADDRINT* retIP,
    ADDRINT retValue,
    ADDRINT HeapHandle, ADDRINT Flags, ADDRINT Size)
{
    if (instrumentOnlyMain && !IsMainExecutable(*retIP)) return;

    UINT32 ctxId = callId * 100 + fcnCallId;
    CallContext* pendingCallContext = TakeCallContext(callContextMap, { callId, tid });
    if (pendingCallContext != nullptr) {
        CallContext* cc = pendingCallContext;
        std::stringstream& ss = cc->stringStream;

        ss << "    Returned pointer (allocated address): 0x"
            << std::hex << retValue << std::dec << '\n';

        if (retValue != 0)
            ss << "    Allocation completed successfully.\n";
        else {
            using namespace WindowsAPI;
            DWORD err = GetLastError();
            ss << "    Allocation failed. GetLastError = " << err << '\n';
        }

        ss << "  [-] Call from RtlAllocateHeap completed\n"
            << "[*] Completed\n\n";

        ExecutionInformation executionCompletedInfo = { ss.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete static_cast<RtlAllocateHeapArgs*>(cc->functionArgs);
        delete cc;
    }
    fcnCallId++;
}

VOID InstRtlAllocateHeap::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "RtlAllocateHeap") return;

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // NTAPI
    // PVOID RtlAllocateHeap(
    //      PVOID  HeapHandle,
    //      ULONG  Flags,
    //      SIZE_T Size );

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // HeapHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Flags
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // Size
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCRET_EXITPOINT_VALUE,     // ponteiro retornado
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // HeapHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // Flags
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // Size
        IARG_END);

    RTN_Close(rtn);
}
