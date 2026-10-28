#include "InstRegQueryValueExA.h"
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>

CallContextMap InstRegQueryValueExA::callContextMap;
UINT32 InstRegQueryValueExA::imgCallId = 0;
CallSequenceNumber InstRegQueryValueExA::fcnCallId = 0;
Notifier* InstRegQueryValueExA::globalNotifierPtr;

/* =========================================================================================
   Helpers de sanitização (similar ao RSMB sanitizer)
   ========================================================================================= */

static inline std::string pad_or_trunc(const std::string& s, size_t len)
{
    if (s.size() == len) return s;
    if (s.size() < len) {
        std::string out = s;
        out.append(len - s.size(), ' ');
        return out;
    }
    return s.substr(0, len);
}

// compara ASCII case-insensitive
static inline bool ieq_ascii(char a, char b)
{
    return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
}

// procura needle (ASCII, case-insensitive) em [buf, buflen); retorna offset ou (size_t)-1
static size_t ci_memmem(const char* buf, size_t buflen, const char* needle, size_t nlen)
{
    if (!buf || !needle || !nlen || buflen < nlen) return (size_t)-1;
    for (size_t i = 0; i + nlen <= buflen; ++i) {
        bool ok = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (!ieq_ascii(buf[i + j], needle[j])) { ok = false; break; }
        }
        if (ok) return i;
    }
    return (size_t)-1;
}

// aplica replace in-place, mantendo comprimento do needle; retorna número de patches
static size_t replace_keyword_inplace(ADDRINT base, size_t buflen,
    const char* needle,
    const char* replacement,
    std::stringstream& log)
{
    if (!base || !buflen || !needle || !replacement) return 0;

    const size_t nlen = std::strlen(needle);
    if (!nlen || buflen < nlen) return 0;

    const std::string replPadded = pad_or_trunc(replacement, nlen);
    size_t patched = 0;

    // varre de forma robusta: recomeça após o fim do patch
    size_t pos = 0;
    while (pos + nlen <= buflen) {
        size_t off = ci_memmem(reinterpret_cast<const char*>(base) + pos, buflen - pos, needle, nlen);
        if (off == (size_t)-1) break;
        off += pos;

        // grava in-place com PIN_SafeCopy
        size_t wrote = PIN_SafeCopy(reinterpret_cast<void*>(base + off), replPadded.data(), nlen);
        if (wrote == nlen) ++patched;

        pos = off + nlen;
    }

    if (patched) {
        log << "    [Countermeasure/REG] \"" << needle << "\" -> \""
            << pad_or_trunc(replacement, nlen) << "\" (kept len " << nlen
            << ") x" << patched << "\n";
    }
    return patched;
}

// sanitiza REG_SZ / REG_EXPAND_SZ: buffer é uma única string ANSI (terminador incluído em lpcbData na prática)
static size_t sanitize_single_sz(ADDRINT base, DWORD cb, std::stringstream& log)
{
    size_t total = 0;

    total += replace_keyword_inplace(base, cb, "innotek GmbH", "PhoenixBIOS", log);
    total += replace_keyword_inplace(base, cb, "VirtualBox", "Dell BIOS", log);
    total += replace_keyword_inplace(base, cb, "VMware", "DELL  ", log);
    total += replace_keyword_inplace(base, cb, "QEMU", "ASUS", log);
    total += replace_keyword_inplace(base, cb, "VBOX", "DELL", log);
    total += replace_keyword_inplace(base, cb, "Virtual Machine", "Workstation    ", log);
    total += replace_keyword_inplace(base, cb, "Virtual", "Genuine", log);
    total += replace_keyword_inplace(base, cb, "Oracle VM", "Dell BIOS", log);

    // datas “clássicas” de VMs (ajuste conforme quiser)
    total += replace_keyword_inplace(base, cb, "12/01/2006", "06/23/2017", log);
    total += replace_keyword_inplace(base, cb, "06/23/99", "06/23/17", log);

    return total;
}

// sanitiza REG_MULTI_SZ: várias strings ANSI, separadas por '\0', bloco termina com '\0\0'
static size_t sanitize_multi_sz(ADDRINT base, DWORD cb, std::stringstream& log)
{
    if (!base || cb < 2) return 0;
    size_t total = 0;

    size_t off = 0;
    while (off < cb) {
        // fim do bloco?
        if (*reinterpret_cast<const char*>(base + off) == '\0') break;

        // tamanho da string corrente (até '\0' ou fim do buffer)
        size_t len = 0;
        while (off + len < cb && *reinterpret_cast<const char*>(base + off + len) != '\0') ++len;
        if (len == 0) break;

        // sanitiza apenas a janela da string corrente
        total += replace_keyword_inplace(base + off, len, "innotek GmbH", "PhoenixBIOS", log);
        total += replace_keyword_inplace(base + off, len, "VirtualBox", "Dell BIOS", log);
        total += replace_keyword_inplace(base + off, len, "VMware", "DELL  ", log);
        total += replace_keyword_inplace(base + off, len, "QEMU", "ASUS", log);
        total += replace_keyword_inplace(base + off, len, "VBOX", "DELL", log);
        total += replace_keyword_inplace(base + off, len, "Virtual Machine", "Workstation    ", log);
        total += replace_keyword_inplace(base + off, len, "Virtual", "Genuine", log);
        total += replace_keyword_inplace(base + off, len, "Oracle VM", "Dell BIOS", log);

        total += replace_keyword_inplace(base + off, len, "12/01/2006", "06/23/2017", log);
        total += replace_keyword_inplace(base + off, len, "06/23/99", "06/23/17", log);

        // pular terminador '\0'
        off += len + 1;
    }
    return total;
}

/* ========================================================================================= */

VOID InstRegQueryValueExA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx,
    ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved,
    ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    auto* args = new InstRegQueryValueExAArgs;
    args->hKey = hKey;
    args->lpValueName = lpValueName;
    args->lpReserved = lpReserved;
    args->lpType = lpType;
    args->lpData = lpData;
    args->lpcbData = lpcbData;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);
}

VOID InstRegQueryValueExA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx,
    ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved,
    ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {

        // Só usamos o lock para IMG/RTN lookups
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = pendingCallContext;
        const auto* args = reinterpret_cast<InstRegQueryValueExAArgs*>(callContext->functionArgs);
        std::stringstream& ss = callContext->stringStream;

        const char* namePtr = reinterpret_cast<const char*>(args->lpValueName);
        std::string valueName = namePtr ? std::string(namePtr) : std::string("<null>");

        ss << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        ss << "    Module name: " << IMG_Name(img) << std::endl;
        ss << "    Thread: " << tid << std::endl;
        ss << "    Call Id: " << fcnCallId << std::endl;
        ss << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        ss << "    Parameters: " << std::endl;
        ss << "        hKey: " << std::hex << (HANDLE)args->hKey << std::dec << std::endl;
        ss << "        lpValueName: " << valueName << std::endl;
        ss << "        lpReserved: " << args->lpReserved << std::endl;
        ss << "        lpType: " << args->lpType << std::endl;
        ss << "        lpData: " << args->lpData << std::endl;
        ss << "        lpcbData: " << args->lpcbData << std::endl;
        ss << "    Return Value: " << retValue << std::endl;

        // ---------------------------
        // CONTRAMEDIDA: sanitizer por palavras-chave (in-place, mantendo comprimento)
        // ---------------------------
        auto ieq_name = [](const std::string& a, const char* b)->bool {
            if (!b) return false;
            size_t nb = std::strlen(b);
            if (a.size() != nb) return false;
            for (size_t i = 0; i < nb; ++i)
                if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
            return true;
            };

        const bool isTarget =
            ieq_name(valueName, "DriverDesc") ||
            ieq_name(valueName, "SystemBiosVersion") ||
            ieq_name(valueName, "VideoBiosVersion");

        const bool needDefend = true; // feature flag, deixe assim por ora

        if (retValue == ERROR_SUCCESS && isTarget && needDefend) {
            DWORD typeVal = 0, cbBytes = 0;
            if (args->lpType)   PIN_SafeCopy(&typeVal, reinterpret_cast<const void*>(args->lpType), sizeof(typeVal));
            if (args->lpcbData) PIN_SafeCopy(&cbBytes, reinterpret_cast<const void*>(args->lpcbData), sizeof(cbBytes));

            // tipos string (ANSI) suportados nesta A-API
            const DWORD REG_SZ_ = 1;
            const DWORD REG_EXPAND_SZ_ = 2;
            const DWORD REG_MULTI_SZ_ = 7;

            if (args->lpData && cbBytes >= 1) {
                size_t totalPatches = 0;

                if (ieq_name(valueName, "DriverDesc")) {
                    totalPatches = sanitize_single_sz(args->lpData, cbBytes, ss);
                }
                else if (ieq_name(valueName, "SystemBiosVersion") || ieq_name(valueName, "VideoBiosVersion")) {
                    totalPatches = sanitize_multi_sz(args->lpData, cbBytes, ss);
                }
                else {
                    // Alguns protetores retornam tipo “estranho”; se quiser, force tratar como string:
                    // totalPatches = sanitize_single_sz(args->lpData, cbBytes, ss);
                    ss << "    [Countermeasure/REG] Skipped (non-string type: " << typeVal << ")\n";
                }

                if (totalPatches == 0) {
                    ss << "    [Countermeasure/REG] No VM strings found to patch.\n";
                }
                else {
                    ss << "    [Countermeasure/REG] Total patches: " << totalPatches << "\n";
                }
            }
            else {
                ss << "    [Countermeasure/REG] Skipped (no buffer or size=0)\n";
            }
        }

        ss << "[*] Completed" << std::endl << std::endl;

        // Notifica observers
        ExecutionInformation info = { ss.str() };
        ExecutionEventData ev(info);
        globalNotifierPtr->NotifyAll(&ev);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstRegQueryValueExA::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RegQueryValueExA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                            // endereço do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,          // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,          // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,          // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,          // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,          // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,          // lpcbData
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,              // ret (ERROR_SUCCESS==0)
            IARG_REG_VALUE, REG_STACK_PTR,             // “endereço de retorno” (conforme sua preferência)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,          // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,          // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,          // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,          // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,          // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,          // lpcbData
            IARG_END);

        RTN_Close(rtn);
    }
}
