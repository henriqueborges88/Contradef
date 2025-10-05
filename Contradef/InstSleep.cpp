#include "InstSleep.h"

/* ---------- variáveis estáticas ---------- */
std::map<CallContextKey, CallContext*> InstSleep::callContextMap;
UINT32 InstSleep::imgCallId = 0;
UINT32 InstSleep::fcnCallId = 0;
Notifier* InstSleep::globalNotifierPtr = nullptr;

/* ---------- BEFORE ---------- */
VOID InstSleep::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT returnAddress,
    ADDRINT dwMilliseconds)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) return;

    SleepArgs* args = new SleepArgs{ dwMilliseconds };
    UINT32 ctxId = callId * 100 + fcnCallId;

    auto* cc = new CallContext(ctxId, tid, inst, args);
    callContextMap[{ctxId, tid}] = cc;

    std::stringstream& ss = cc->stringStream;
    ss << "\n[+] Sleep...\n"
        << "    Thread: " << tid << '\n'
        << "    Caller id: " << fcnCallId << '\n'
        << "    Routine address: 0x" << std::hex << cc->rtnAddress << std::dec << '\n'
        << "    Parameters:\n"
        << "        dwMilliseconds: " << dwMilliseconds << " ms\n"
        << "    Address of the calling function: 0x" << std::hex << returnAddress << std::dec << '\n'
        << "  [-] Start of Sleep call\n";

}

/* ---------- AFTER ---------- */
VOID InstSleep::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT* returnAddress,
    ADDRINT dwMilliseconds)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) return;

    UINT32 ctxId = callId * 100 + fcnCallId;
    auto it = callContextMap.find({ ctxId, tid });
    if (it != callContextMap.end()) {
        CallContext* cc = it->second;
        std::stringstream& ss = cc->stringStream;

        // Sleep é VOID — apenas indicamos conclusão
        ss << "    Operation completed (Sleep returned).\n"
            << "  [-] Sleep call completed\n"
            << "[*] Completed\n\n";

        globalNotifierPtr->NotifyAll(new ExecutionEventData({ ss.str() }));

        delete reinterpret_cast<SleepArgs*>(cc->functionArgs);
        delete cc;
        callContextMap.erase(it);
    }
    fcnCallId++;
}

/* ---------- Instrumentação ---------- */
VOID InstSleep::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "Sleep") return;      // só interessa a função Sleep de kernel32.dll

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // VOID Sleep(DWORD dwMilliseconds);

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,   // dwMilliseconds
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,   // dwMilliseconds (para registro)
        IARG_END);

    RTN_Close(rtn);
}
