#include "InstLstrcmpiA.h"

std::map<CallContextKey, CallContext*> InstLstrcmpiA::callContextMap;
UINT32 InstLstrcmpiA::imgCallId = 0;
UINT32 InstLstrcmpiA::fcnCallId = 0;
Notifier* InstLstrcmpiA::globalNotifierPtr = nullptr;

/* ---------- BEFORE ---------- */
VOID InstLstrcmpiA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT retIP,
    ADDRINT lpString1, ADDRINT lpString2)
{
    if (instrumentOnlyMain && !IsMainExecutable(retIP)) return;

    LstrcmpiAArgs* args = new LstrcmpiAArgs{ lpString1, lpString2 };
    UINT32 ctxId = callId * 100 + fcnCallId;

    auto* cc = new CallContext(ctxId, tid, inst, args);
    callContextMap[{ctxId, tid}] = cc;

    auto readStr = [](ADDRINT ptr) -> std::string {
        if (!ptr) return "NULL";
        std::string s(256, '\0');
        WindowsAPI::SIZE_T r = PIN_SafeCopy(&s[0], reinterpret_cast<char*>(ptr), 255);
        s[r] = '\0';
        return s;
        };

    std::stringstream& s = cc->stringStream;
    s << "\n[+] lstrcmpiA...\n"
        << "    Thread: " << tid << "\n"
        << "    Caller id: " << fcnCallId << "\n"
        << "    Routine address: 0x" << std::hex << cc->rtnAddress << std::dec << "\n"
        << "    Parameters:\n"
        << "        lpString1: " << readStr(lpString1) << "\n"
        << "        lpString2: " << readStr(lpString2) << "\n"
        << "    Address of the calling function: 0x" << std::hex << retIP << std::dec << "\n"
        << "  [-] Start of lstrcmpiA call\n";

}

/* ---------- AFTER ---------- */
VOID InstLstrcmpiA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT /*rtn*/, CONTEXT*,
    ADDRINT* retIP,
    ADDRINT retValue,
    ADDRINT lpString1, ADDRINT lpString2)
{
    if (instrumentOnlyMain && !IsMainExecutable(*retIP)) return;

    UINT32 ctxId = callId * 100 + fcnCallId;
    auto it = callContextMap.find({ ctxId, tid });
    if (it != callContextMap.end()) {
        CallContext* cc = it->second;
        std::stringstream& s = cc->stringStream;

        int cmp = static_cast<int>(retValue);
        s << "    Returned from lstrcmpiA: " << cmp << " (";
        if (cmp == 0)       s << "equal sequence";
        else if (cmp < 0)   s << "string1 < string2";
        else                s << "string1 > string2";
        s << ")\n";

        s << "  [-] Call from lstrcmpiA completed\n"
            << "[*] Completed\n\n";

        globalNotifierPtr->NotifyAll(new ExecutionEventData({ s.str() }));

        delete reinterpret_cast<LstrcmpiAArgs*>(cc->functionArgs);
        delete cc;
        callContextMap.erase(it);
    }
    fcnCallId++;
}

/* ---------- Instrumentação ---------- */
VOID InstLstrcmpiA::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "lstrcmpiA") return;

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // int lstrcmpiA(LPCSTR lpString1, LPCSTR lpString2);

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpString1
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString2
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCRET_EXITPOINT_VALUE,     // int retorno
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpString1
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpString2
        IARG_END);

    RTN_Close(rtn);
}
