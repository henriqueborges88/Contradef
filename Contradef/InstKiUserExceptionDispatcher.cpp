#include "InstKiUserExceptionDispatcher.h"

std::map<CallContextKey, CallContext*> InstKiUserExceptionDispatcher::callContextMap;
UINT32 InstKiUserExceptionDispatcher::imgCallId = 0;
UINT32 InstKiUserExceptionDispatcher::fcnCallId = 0;
Notifier* InstKiUserExceptionDispatcher::globalNotifierPtr = nullptr;

// ------------------------------------------------------------
// Utilitários p/ leitura rápida do EXCEPTION_RECORD (genérico x86/x64)
// ------------------------------------------------------------
static bool ReadExceptionRecordSummary(ADDRINT pER,
    WindowsAPI::DWORD& code,
    WindowsAPI::DWORD& flags,
    ADDRINT& exceptionAddr,
    WindowsAPI::DWORD& numParams,
    std::vector<ADDRINT>& firstParams /*até 4*/)
{
    if (pER == 0) return false;

    const size_t ptrSize = sizeof(ADDRINT);

    // Layout:
    // +0  DWORD   ExceptionCode
    // +4  DWORD   ExceptionFlags
    // +8  PEXCEPTION_RECORD ExceptionRecord
    // +8+ptr   PVOID  ExceptionAddress
    // +8+2*ptr DWORD  NumberParameters
    // +8+2*ptr+(ptr==8?8:4)  ExceptionInformation[]
    size_t offCode = 0;
    size_t offFlags = 4;
    size_t offAddr = 8 + ptrSize;
    size_t offNum = 8 + 2 * ptrSize;
    size_t offInfo = 8 + 2 * ptrSize + ((ptrSize == 8) ? 8 : 4);

    code = flags = numParams = 0;
    exceptionAddr = 0;
    firstParams.clear();

    size_t copied = PIN_SafeCopy(&code, reinterpret_cast<void*>(pER + offCode), sizeof(code));
    if (copied != sizeof(code)) return false;

    copied = PIN_SafeCopy(&flags, reinterpret_cast<void*>(pER + offFlags), sizeof(flags));
    if (copied != sizeof(flags)) return false;

    copied = PIN_SafeCopy(&exceptionAddr, reinterpret_cast<void*>(pER + offAddr), ptrSize);
    if (copied != ptrSize) return false;

    copied = PIN_SafeCopy(&numParams, reinterpret_cast<void*>(pER + offNum), sizeof(numParams));
    if (copied != sizeof(numParams)) return false;

    WindowsAPI::DWORD toRead = numParams;
    if (toRead > 4) toRead = 4;

    for (WindowsAPI::DWORD i = 0; i < toRead; ++i) {
        ADDRINT v = 0;
        size_t c = PIN_SafeCopy(&v, reinterpret_cast<void*>(pER + offInfo + i * ptrSize), ptrSize);
        if (c == ptrSize) firstParams.push_back(v);
        else break;
    }
    return true;
}

// ------------------------------------------------------------
// CONTRAMEDIDA: patch de EXCEPTION_RECORD no dispatcher
// ------------------------------------------------------------
//static const WindowsAPI::DWORD EXCEPTION_NONCONTINUABLE = 0x1;
//static const WindowsAPI::DWORD DBG_PRINTEXCEPTION_C = 0x40010006; // benigno/continuável

static inline bool IsWhitelisted(DWORD c)
{
    return c == 0xE06D7363u      // C++ EH
        || c == 0xE0434352u      // .NET
        || c == 0x406D1388u;     // Thread name
}

static inline bool IsSuspicious(DWORD c)
{
    return c == 0xC0000409u      // STATUS_STACK_BUFFER_OVERRUN / fail-fast
        || c == 0xC000001Du      // ILLEGAL_INSTRUCTION
        || c == 0xC0000096u;     // PRIVILEGED_INSTRUCTION
    // se desejar, acrescente 0xC0000005 (AV) quando usada como anti-debug
}

static std::map<CallContextKey, PatchInfo> g_patchInfo;

static bool PatchExceptionRecordIfNeeded(ADDRINT pER, PatchInfo& outInfo)
{
    outInfo = PatchInfo{};
    if (!pER) return false;

    const size_t ptrSize = sizeof(ADDRINT);
    const size_t offCode = 0;
    const size_t offFlags = 4;
    const size_t offAddr = 8 + ptrSize;                         // não precisamos escrever
    const size_t offNum = 8 + 2 * ptrSize;
    const size_t offInfo = 8 + 2 * ptrSize + ((ptrSize == 8) ? 8 : 4);

    WindowsAPI::DWORD code = 0, flags = 0, num = 0;
    ADDRINT dummyAddr = 0;
    std::vector<ADDRINT> dummy;
    if (!ReadExceptionRecordSummary(pER, code, flags, dummyAddr, num, dummy))
        return false;

    outInfo.inspected = true;
    outInfo.oldCode = code;
    outInfo.oldFlags = flags;
    outInfo.oldNum = num;

    const bool noncont = (flags & EXCEPTION_NONCONTINUABLE) != 0;
    if (IsWhitelisted(code) || (!IsSuspicious(code) && !noncont)) {
        // Nada a fazer
        return false;
    }

    // Patch: torna exceção benigna e continuável
    WindowsAPI::DWORD newCode = DBG_PRINTEXCEPTION_C;
    WindowsAPI::DWORD newFlags = 0;
    WindowsAPI::DWORD newNum = 0;

    size_t w1 = PIN_SafeCopy(reinterpret_cast<void*>(pER + offCode), &newCode, sizeof(newCode));
    size_t w2 = PIN_SafeCopy(reinterpret_cast<void*>(pER + offFlags), &newFlags, sizeof(newFlags));
    size_t w3 = PIN_SafeCopy(reinterpret_cast<void*>(pER + offNum), &newNum, sizeof(newNum));

    // Zera payload existente (até 15 entradas)
    const WindowsAPI::DWORD maxEI = (num > 15 ? 15 : num);
    for (WindowsAPI::DWORD i = 0; i < maxEI; ++i) {
        UINT64 z = 0;
        PIN_SafeCopy(reinterpret_cast<void*>(pER + offInfo + i * ptrSize), &z, ptrSize);
    }

    const bool ok = (w1 == sizeof(newCode) && w2 == sizeof(newFlags) && w3 == sizeof(newNum));
    if (ok) {
        outInfo.patched = true;
        outInfo.newCode = newCode;
        outInfo.newFlags = newFlags;
        outInfo.newNum = newNum;
    }
    return ok;
}

// ------------------------------------------------------------

VOID InstKiUserExceptionDispatcher::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT returnAddress,
    ADDRINT pExceptionRecord,
    ADDRINT pContext)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    auto* args = new KiUserExceptionDispatcherArgs;
    args->pExceptionRecord = pExceptionRecord;
    args->pContext = pContext;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // -------------------------------
    // CONTRAMEDIDA: patch ANTES do dispatcher
    // -------------------------------
    PatchInfo pi{};
    if (PatchExceptionRecordIfNeeded(pExceptionRecord, pi)) {
        g_patchInfo[key] = pi; // guardamos p/ log no AFTER
    }
    else if (pi.inspected) {
        g_patchInfo[key] = pi; // inspecionado mas não precisou patch
    }
}

VOID InstKiUserExceptionDispatcher::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT* returnAddress,
    ADDRINT pExceptionRecord,
    ADDRINT pContext)
{
    // Filtra apenas chamadas vindas do módulo principal (novo padrão usando *returnAddress)
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = it->second;
        const KiUserExceptionDispatcherArgs* args =
            reinterpret_cast<KiUserExceptionDispatcherArgs*>(callContext->functionArgs);

        std::stringstream& ss = callContext->stringStream;

        // Extrai um resumo do EXCEPTION_RECORD (após possível patch)
        WindowsAPI::DWORD excCode = 0, excFlags = 0, numParams = 0;
        ADDRINT excAddr = 0;
        std::vector<ADDRINT> firstParams;
        bool haveER = ReadExceptionRecordSummary(args->pExceptionRecord, excCode, excFlags, excAddr, numParams, firstParams);

        ss << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        ss << "    Module name: " << IMG_Name(img) << std::endl;
        ss << "    Thread: " << tid << std::endl;
        ss << "    Call Id: " << fcnCallId << std::endl;
        ss << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        ss << "    Parameters: " << std::endl;
        ss << "        ExceptionRecord*: 0x" << std::hex << args->pExceptionRecord << std::dec << std::endl;

        if (haveER) {
            ss << "            ExceptionCode: 0x" << std::hex << excCode << std::dec << std::endl;
            ss << "            ExceptionFlags: 0x" << std::hex << excFlags << std::dec << std::endl;
            ss << "            ExceptionAddress: 0x" << std::hex << excAddr << std::dec << std::endl;
            ss << "            NumberParameters: " << numParams << std::endl;
            if (!firstParams.empty()) {
                ss << "            ExceptionInformation[0.."
                    << (firstParams.size() - 1) << "]: ";
                for (size_t i = 0; i < firstParams.size(); ++i) {
                    ss << "0x" << std::hex << firstParams[i] << std::dec;
                    if (i + 1 < firstParams.size()) ss << ", ";
                }
                ss << std::endl;
            }
        }
        else {
            ss << "            (não foi possível ler um resumo do EXCEPTION_RECORD)" << std::endl;
        }

        ss << "        Context*: 0x" << std::hex << args->pContext << std::dec << std::endl;

        // Log da contramedida, se aplicável
        auto pit = g_patchInfo.find(key);
        if (pit != g_patchInfo.end() && pit->second.inspected) {
            const PatchInfo& pi = pit->second;
            if (pi.patched) {
                ss << "    [Countermeasure] Patched exception: "
                    << "Code 0x" << std::hex << pi.oldCode
                    << "→0x" << pi.newCode
                    << ", Flags 0x" << pi.oldFlags
                    << "→0x" << pi.newFlags
                    << ", Num " << std::dec << pi.oldNum
                    << "→" << pi.newNum << std::endl;
            }
            else {
                ss << "    [Countermeasure] No patch (whitelisted or continuable)" << std::endl;
            }
            g_patchInfo.erase(pit);
        }

        // KiUserExceptionDispatcher é VOID — não há valor de retorno
        ss << "    Return: (void)" << std::endl;
        ss << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation info = { ss.str() };
        ExecutionEventData event(info);
        globalNotifierPtr->NotifyAll(&event);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstKiUserExceptionDispatcher::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "KiUserExceptionDispatcher") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // VOID KiUserExceptionDispatcher(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT Context);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                         // endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,       // pExceptionRecord
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,       // pContext
            IARG_END);

        // AFTER: função VOID — não usamos IARG_FUNCRET_EXITPOINT_VALUE
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR,          // ponteiro para o endereço de retorno
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,       // pExceptionRecord
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,       // pContext
            IARG_END);

        RTN_Close(rtn);
    }
}
