#include "InstNtQueryObject.h"

CallContextMap InstNtQueryObject::callContextMap;
UINT32 InstNtQueryObject::imgCallId = 0;
CallSequenceNumber InstNtQueryObject::fcnCallId = 0;
Notifier* InstNtQueryObject::globalNotifierPtr = nullptr;

/* ---------- BEFORE ---------- */
VOID InstNtQueryObject::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT retIP,
    ADDRINT Handle, ADDRINT ObjectInformationClass,
    ADDRINT ObjectInformation, ADDRINT ObjectInformationLength,
    ADDRINT ReturnLength)
{
    if (instrumentOnlyMain && !IsMainExecutable(retIP)) return;

    NtQueryObjectArgs args{ Handle, ObjectInformationClass, ObjectInformation,
                           ObjectInformationLength, ReturnLength };
    UINT32 ctxId = callId * 100 + fcnCallId;

    auto* cc = new CallContext(ctxId, tid, inst, &args);
    SaveCallContext(callContextMap, {callId, tid}, cc);

    std::stringstream& s = cc->stringStream;
    s << "\n[+] NtQueryObject...\n"
        << "    Thread: " << tid << "\n"
        << "    Call Id: " << fcnCallId << "\n"
        << "    Routine address: 0x" << std::hex << cc->rtnAddress << std::dec << "\n"
        << "    Parameters:\n"
        << "        Handle: 0x" << std::hex << Handle << std::dec << "\n"
        << "        ObjectInformationClass: " << ObjectInformationClass << "\n"
        << "        ObjectInformation: 0x" << std::hex << ObjectInformation << std::dec << "\n"
        << "        ObjectInformationLength: " << ObjectInformationLength << "\n"
        << "        ReturnLength: 0x" << std::hex << ReturnLength << std::dec << "\n"
        << "    Address of the calling function: 0x" << std::hex << retIP << std::dec << "\n"
        << "  [-] Start of NtQueryObject call\n";

}

/* ---------- AFTER ---------- */
VOID InstNtQueryObject::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT* retIP,
    ADDRINT retValue,
    ADDRINT Handle, ADDRINT ObjectInformationClass,
    ADDRINT ObjectInformation, ADDRINT ObjectInformationLength,
    ADDRINT ReturnLength)
{
    if (instrumentOnlyMain && !IsMainExecutable(*retIP)) return;

    UINT32 ctxId = callId * 100 + fcnCallId;
    CallContext* pendingCallContext = TakeCallContext(callContextMap, { callId, tid });
    if (pendingCallContext != nullptr) {
        CallContext* cc = pendingCallContext;
        std::stringstream& s = cc->stringStream;

        LONG status = static_cast<LONG>(retValue);
        s << "    Return value from NtQueryObject (NTSTATUS): 0x" << std::hex << status << std::dec << "\n";

        if (status == 0) {                       // STATUS_SUCCESS
            s << "    Successful operation.\n";
            if (ReturnLength) {
                ULONG outLen = 0;
                if (PIN_SafeCopy(&outLen, reinterpret_cast<ULONG*>(ReturnLength), sizeof(ULONG))
                    == sizeof(ULONG))
                    s << "    ResultLength: " << outLen << " bytes\n";
            }
            if (ObjectInformation && ObjectInformationLength) {
                ULONG show = (ObjectInformationLength < 64 ? ObjectInformationLength : 64);
                std::vector<BYTE> buf(show);
                WindowsAPI::SIZE_T copied = PIN_SafeCopy(&buf[0],
                    reinterpret_cast<BYTE*>(ObjectInformation),
                    show);
                s << "    First " << copied
                    << " bytes of ObjectInformation: ";
                for (WindowsAPI::SIZE_T i = 0; i < copied; ++i)
                    s << std::hex << (int)buf[i] << " ";
                s << std::dec << "\n";
            }
        }
        else {
            s << "    Operation failed.\n";
        }

        s << "  [-] Call NtQueryObject Completed\n"
            << "[*] Completed\n\n";

        globalNotifierPtr->NotifyAll(new ExecutionEventData({ s.str() }));

        delete cc;
    }
    fcnCallId++;
}

/* ---------- Instrumentação ---------- */
VOID InstNtQueryObject::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "NtQueryObject") return;

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // NTSTATUS NtQueryObject(
    //     HANDLE                   Handle,
    //     OBJECT_INFORMATION_CLASS ObjectInformationClass,
    //     PVOID                    ObjectInformation,
    //     ULONG                    ObjectInformationLength,
    //     PULONG                   ReturnLength );

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ObjectInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ObjectInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCRET_EXITPOINT_VALUE,     // NTSTATUS
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // Handle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ObjectInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ObjectInformationLength
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
        IARG_END);

    RTN_Close(rtn);
}
