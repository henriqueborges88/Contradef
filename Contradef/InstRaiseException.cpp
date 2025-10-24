#include "InstRaiseException.h"

// =================== Config ===================
static const bool kEnableCountermeasure = false;
static const bool kAggressiveAV = false; // se true, também neutraliza 0xC0000005 quando for levantada via RaiseException.
// =============================================

std::map<CallContextKey, CallContext*> InstRaiseException::callContextMap;
UINT32 InstRaiseException::imgCallId = 0;
UINT32 InstRaiseException::fcnCallId = 0;
Notifier* InstRaiseException::globalNotifierPtr;

static inline bool IsCodeToKeep(DWORD code) {
    // C++ EH / .NET / MS VC thread-name => deixar passar
    return (code == 0xE06D7363u  // C++ EH
        || code == 0xE0434352u  // .NET EH
        || code == 0x406D1388u  // MS_VC_EXCEPTION (thread name)
        );
}

static inline bool IsCodeSuspicious(DWORD code) {
    if (code == 0xC0000409u   // STATUS_STACK_BUFFER_OVERRUN / fail-fast
        || code == 0xC000001Du   // ILLEGAL_INSTRUCTION
        || code == 0xC0000096u)  // PRIVILEGED_INSTRUCTION
        return true;

    if (kAggressiveAV && code == 0xC0000005u) // AV levantada via RaiseException (padrão anti-debug)
        return true;

    // Muitos packers usam códigos “custom”. Se quiser, acrescente aqui uma whitelist/blacklist.
    return false;
}

VOID InstRaiseException::CallbackBefore(
    THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT dwExceptionCode, ADDRINT dwExceptionFlags, ADDRINT nNumberOfArguments, ADDRINT lpArguments,
    ADDRINT* pCodeRef, ADDRINT* pFlagsRef, ADDRINT* pNArgsRef, ADDRINT* pArgsRef) // <- REFERÊNCIAS
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    auto* args = new RaiseExceptionArgs;
    args->dwExceptionCode = dwExceptionCode;
    args->dwExceptionFlags = dwExceptionFlags;
    args->nNumberOfArguments = nNumberOfArguments;
    args->lpArguments = lpArguments;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& ss = callContext->stringStream;
    ss << "\n[+] RaiseException...\n"
        << "    Thread: " << tid << "\n"
        << "    Call Id: " << fcnCallId << "\n"
        << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << "\n"
        << "    Parameters:\n"
        << "        dwExceptionCode: 0x" << std::hex << (DWORD)dwExceptionCode << std::dec << "\n"
        << "        dwExceptionFlags: 0x" << std::hex << (DWORD)dwExceptionFlags << std::dec << "\n"
        << "        nNumberOfArguments: " << (DWORD)nNumberOfArguments << "\n"
        << "        lpArguments: 0x" << std::hex << lpArguments << std::dec << "\n"
        << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << "\n";

    // (Opcional) Dump de até 8 argumentos
    if (nNumberOfArguments > 0 && lpArguments != 0) {
        WindowsAPI::ULONG_PTR tmp[8] = {};
        size_t count = (nNumberOfArguments > 8) ? 8 : (size_t)nNumberOfArguments;
        PIN_SafeCopy(tmp, reinterpret_cast<const VOID*>(lpArguments), count * sizeof(WindowsAPI::ULONG_PTR));
        for (size_t i = 0; i < count; ++i) {
            ss << "        Argument[" << i << "]: 0x" << std::hex << tmp[i] << std::dec << "\n";
        }
    }

    // ==========================
    // CONTRAMEDIDA (patch in-place)
    // ==========================
    if (kEnableCountermeasure) {
        const DWORD code = (DWORD)dwExceptionCode;
        const DWORD flags = (DWORD)dwExceptionFlags;

        const bool nonContinuable = (flags & 0x1u) != 0; // EXCEPTION_NONCONTINUABLE
        const bool suspicious = IsCodeSuspicious(code);
        const bool keep = IsCodeToKeep(code);

        if (!keep && (nonContinuable || suspicious)) {
            // Transformar em exceção benigna e continuável
            const DWORD benignCode = 0x40010006u; // DBG_PRINTEXCEPTION_C
            const DWORD benignFlags = 0;
            const DWORD benignNArgs = 0;

            // Gravar LOG de originais
            args->patched = true;
            args->origCode = code;
            args->origFlags = flags;
            args->origNArgs = (DWORD)nNumberOfArguments;
            args->origArgsPtr = lpArguments;
            args->newCode = benignCode;
            args->newFlags = benignFlags;
            args->newNArgs = benignNArgs;

            // Patch efetivo (ref)
            if (pCodeRef)  *pCodeRef = (ADDRINT)benignCode;
            if (pFlagsRef) *pFlagsRef = (ADDRINT)benignFlags;
            if (pNArgsRef) *pNArgsRef = (ADDRINT)benignNArgs;
            if (pArgsRef)  *pArgsRef = (ADDRINT)0;

            ss << "    [Countermeasure] Neutralized RaiseException: "
                << "code 0x" << std::hex << code << " -> 0x" << benignCode
                << ", flags 0x" << flags << " -> 0x" << benignFlags
                << ", nArgs " << std::dec << (DWORD)nNumberOfArguments << " -> " << benignNArgs
                << ", lpArgs -> NULL\n";
        }
    }

    ss << "  [-] Start of call RaiseException\n";
}

VOID InstRaiseException::CallbackAfter(
    THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT dwExceptionCode, ADDRINT dwExceptionFlags, ADDRINT nNumberOfArguments, ADDRINT lpArguments)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& ss = callContext->stringStream;

        // RaiseException é VOID
        ss << "    Returned from RaiseException: (VOID)\n";

        // Se houve patch, sumariza
        const RaiseExceptionArgs* a = reinterpret_cast<RaiseExceptionArgs*>(callContext->functionArgs);
        if (a->patched) {
            ss << "    [Countermeasure] Patch summary:\n"
                << "        code: 0x" << std::hex << a->origCode << " -> 0x" << a->newCode << "\n"
                << "        flags: 0x" << a->origFlags << " -> 0x" << a->newFlags << "\n"
                << "        nArgs: " << std::dec << a->origNArgs << " -> " << a->newNArgs << "\n"
                << "        lpArgs: 0x" << std::hex << a->origArgsPtr << " -> 0x0\n";
        }

        ss << "  [-] Call from RaiseException completed\n"
            << "[*] Completed\n\n";

        ExecutionInformation info = { ss.str() };
        ExecutionEventData evt(info);
        globalNotifierPtr->NotifyAll(&evt);

        delete a;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstRaiseException::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) == "RaiseException") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // BEFORE: valores + referências para poder patchar
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,

            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // code (para log)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // flags (para log)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nArgs (para log)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpArgs (para log)

            IARG_FUNCARG_ENTRYPOINT_REFERENCE, 0, // pCode (para patch)
            IARG_FUNCARG_ENTRYPOINT_REFERENCE, 1, // pFlags
            IARG_FUNCARG_ENTRYPOINT_REFERENCE, 2, // pNArgs
            IARG_FUNCARG_ENTRYPOINT_REFERENCE, 3, // pArgs
            IARG_END);

        // AFTER: só para log consistente
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR,  // endereço de retorno do chamador
            IARG_FUNCRET_EXITPOINT_VALUE,   // (void) ignorado
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_END);

        RTN_Close(rtn);
    }
}
