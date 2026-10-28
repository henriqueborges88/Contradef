#include "InstCloseHandle.h"

CallContextMap InstCloseHandle::callContextMap;
UINT32 InstCloseHandle::imgCallId = 0;
CallSequenceNumber InstCloseHandle::fcnCallId = 0;
Notifier* InstCloseHandle::globalNotifierPtr = nullptr;

/* ---------- BEFORE ---------- */
VOID InstCloseHandle::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT returnAddress,
    ADDRINT hObject)
{
    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            return;
        }
    }

    CloseHandleArgs args{ hObject };
    UINT32 ctxId = callId * 100 + fcnCallId;

    auto* cc = new CallContext(ctxId, tid, inst, &args);
    SaveCallContext(callContextMap, {callId, tid}, cc);

    std::stringstream& ss = cc->stringStream;
    ss << "\n[+] CloseHandle...\n"
        << "    Thread: " << tid << '\n'
        << "    Call Id: " << fcnCallId << '\n'
        << "    Routine address: 0x" << std::hex << cc->rtnAddress << std::dec << '\n'
        << "    Parameters:\n"
        << "        hObject: 0x" << std::hex << hObject << std::dec << '\n'
        << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << '\n'
        << "  [-] Start of call CloseHandle\n";

}

/* ---------- AFTER ---------- */
VOID InstCloseHandle::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT* returnAddress,
    ADDRINT retValue,
    ADDRINT hObject)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 ctxId = callId * 100 + fcnCallId;
    CallContext* pendingCallContext = TakeCallContext(callContextMap, { callId, tid });
    if (pendingCallContext != nullptr) {
        CallContext* cc = pendingCallContext;
        std::stringstream& ss = cc->stringStream;

        BOOL result = static_cast<BOOL>(retValue);
        ss << "    Returned from CloseHandle: " << (result ? "TRUE" : "FALSE") << '\n';

        if (!result) {                       // Falha –– exibir GetLastError()
            using namespace WindowsAPI;
            DWORD err = GetLastError();
            ss << "    Failed to close handle 0x" << std::hex << hObject << std::dec
                << ". GetLastError = " << err << '\n';
        }
        else {
            ss << "    Handle closed successfully.\n";
        }

        ss << "  [-] Call from CloseHandle completed\n"
            << "[*] Completed\n\n";

        ExecutionInformation executionCompletedInfo = { ss.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete cc;
    }
    fcnCallId++;
}

/* ---------- Instrumentação ---------- */
VOID InstCloseHandle::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "CloseHandle") return;

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    if (IsStringInStrategyScope("Obsidium")) {
        // fallback para o Obsidium
        size_t scanSize = 0xc;
        ADDRINT base = RTN_Address(rtn);
        for (INS i = RTN_InsHead(rtn);
            INS_Valid(i) && INS_Address(i) <= base + scanSize;
            i = INS_Next(i))
        {

            if (INS_Address(i) == base + scanSize)
            {
                INS_InsertCall(i, IPOINT_BEFORE,
                    (AFUNPTR)CallbackBefore,
                    IARG_THREAD_ID,
                    IARG_UINT32, imgCallId,
                    IARG_INST_PTR,
                    IARG_ADDRINT, RTN_Address(rtn),
                    IARG_CONTEXT,
                    IARG_RETURN_IP,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hObject
                    IARG_END);
            }
        }
    }
    else {
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hObject
            IARG_END);
    }

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante)
        IARG_FUNCRET_EXITPOINT_VALUE,     // BOOL retorno
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hObject
        IARG_END);

    RTN_Close(rtn);
}
