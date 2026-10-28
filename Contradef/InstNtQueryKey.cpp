#include "InstNtQueryKey.h"

CallContextMap InstNtQueryKey::callContextMap;
UINT32 InstNtQueryKey::imgCallId = 0;
CallSequenceNumber InstNtQueryKey::fcnCallId = 0;
Notifier* InstNtQueryKey::globalNotifierPtr = nullptr;

BOOL ReturnsNameString(ULONG cls)
{
    if (cls == 0 /*KeyBasicInformation*/ ||
        cls == 3 /*KeyNameInformation*/ ||
        cls == 6 /*KeyCachedInformation*/) {
        return true;
    }
    else {
        return false;
    }
}

/* ---------- BEFORE ---------- */
VOID InstNtQueryKey::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT retIP,
    ADDRINT KeyHandle, ADDRINT KeyInformationClass,
    ADDRINT KeyInformation, ADDRINT Length, ADDRINT ResultLength)
{
    if (instrumentOnlyMain && !IsMainExecutable(retIP)) return;

    NtQueryKeyArgs* args = new NtQueryKeyArgs;
    args->KeyHandle = KeyHandle;
    args->KeyInformationClass = KeyInformationClass;
    args->KeyInformation = KeyInformation;
    args->Length = Length;
    args->ResultLength = ResultLength;

    UINT32 ctxId = callId * 100 + fcnCallId;

    auto* cc = new CallContext(ctxId, tid, inst, args);
    SaveCallContext(callContextMap, {callId, tid}, cc);

    //std::stringstream& ss = cc->stringStream;
    //ss << "\n[+] NtQueryKey...\n"
    //    << "    Thread: " << tid << '\n'
    //    << "    Id de chamada: " << fcnCallId << '\n'
    //    << "    Endereço da rotina: 0x" << std::hex << cc->rtnAddress << std::dec << '\n'
    //    << "    Parâmetros:\n"
    //    << "        KeyHandle: 0x" << std::hex << KeyHandle << std::dec << '\n'
    //    << "        KeyInformationClass: " << KeyInformationClass << '\n'
    //    << "        KeyInformation: 0x" << std::hex << KeyInformation << std::dec << '\n'
    //    << "        Length: " << Length << '\n'
    //    << "        ResultLength: 0x" << std::hex << ResultLength << std::dec << '\n'
    //    << "    Endereço da função chamante: 0x" << std::hex << retIP << std::dec << '\n'
    //    << "  [-] Início da chamada NtQueryKey\n";

}

/* ---------- AFTER ---------- */
VOID InstNtQueryKey::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT* retIP,
    ADDRINT retValue,
    ADDRINT KeyHandle, ADDRINT KeyInformationClass,
    ADDRINT KeyInformation, ADDRINT Length, ADDRINT ResultLength)
{
    if (instrumentOnlyMain && !IsMainExecutable(*retIP)) return;

    UINT32 ctxId = callId * 100 + fcnCallId;

    CallContext* pendingCallContext = TakeCallContext(callContextMap, { callId, tid });
    if (pendingCallContext != nullptr) {

        PIN_LockClient();
        RTN rtnCurrent = RTN_FindByAddress(inst);
        IMG img = IMG_FindByAddress(inst);
        PIN_UnlockClient();

        CallContext* cc = pendingCallContext;

        const NtQueryKeyArgs* args = reinterpret_cast<NtQueryKeyArgs*>(cc->functionArgs);
    
        std::stringstream& stringStream = cc->stringStream;

        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << cc->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        KeyHandle: " << std::hex << (uintptr_t)args->KeyHandle << std::endl;
        stringStream << "        KeyInformationClass: " << std::dec << args->KeyInformationClass << std::endl;

        /* lê ResultLength ----------------------------------*/
        ULONG realLen = 0;
        PIN_SafeCopy(&realLen, (PULONG)ResultLength, sizeof(realLen));

        if (ReturnsNameString(args->KeyInformationClass) && realLen >= sizeof(UNICODE_STRING))
        {
            /* copia os primeiros bytes só para ler o UNICODE_STRING  */
            UNICODE_STRING us{};
            if (PIN_SafeCopy(&us, (PVOID)KeyInformation, sizeof(us)) == sizeof(us) &&
                us.Length && us.Buffer)
            {
                const size_t wcharCount = us.Length / sizeof(WCHAR);
                std::wstring tmp(wcharCount, L'\0');
                if (PIN_SafeCopy(&tmp[0], us.Buffer, us.Length) == us.Length)
                {
                    stringStream << "        KeyInformation: " << std::hex << WStringToString(tmp) << std::endl;
                }
                else
                {
                    stringStream << "        KeyInformation: [Error copying buffer]" << std::endl;
                }
            }
            else {
                stringStream << "        KeyInformation: [Error copying buffer]";
            }
        }
        else
        {
            /* hexdump (até 64 bytes) ---------------------------*/
            BYTE tmp[64] = {};
            size_t toCopy = (KeyInformation && realLen)
                ? std::min<size_t>(realLen, sizeof(tmp))
                : 0;

            if (toCopy && PIN_SafeCopy(tmp, (PVOID)KeyInformation, toCopy))
            {
                stringStream << "        KeyInformation: ";

                for (size_t i = 0; i < toCopy; ++i)
                    stringStream << std::setw(2) << std::setfill('0') << std::hex
                    << (unsigned)tmp[i] << ' ';
                stringStream << std::dec << std::endl;
            }
            else {
                stringStream << "        KeyInformation: [Error copying buffer]";
            }
        }

        stringStream << "        Length (ptr): 0x" << std::hex << args->Length << std::dec << std::endl;
        stringStream << "        ResultLength: " << realLen << std::endl;
        stringStream << "    Return Value: " << static_cast<LONG>(retValue) << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        globalNotifierPtr->NotifyAll(new ExecutionEventData({ stringStream.str() }));

        delete args;
        delete cc;
    }
    fcnCallId++;
}

/* ---------- Instrumentação ---------- */
VOID InstNtQueryKey::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName != "NtQueryKey" && rtnName != "ZwQueryKey") {
        return;
    }

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // NTSTATUS NtQueryKey(
    //   HANDLE               KeyHandle,
    //   KEY_INFORMATION_CLASS KeyInformationClass,
    //   PVOID                KeyInformation,
    //   ULONG                Length,
    //   PULONG               ResultLength );

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // KeyInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // KeyInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // Length
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ResultLength
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCRET_EXITPOINT_VALUE,     // NTSTATUS
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // KeyInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // KeyInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // Length
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ResultLength
        IARG_END);

    RTN_Close(rtn);
}
