#include "InstGetSystemFirmwareTable.h"
#include <vector>
#include <string>
#include <cstring>
#include <cctype>
#include <algorithm>

// Inicialização das variáveis estáticas
CallContextMap InstGetSystemFirmwareTable::callContextMap;
UINT32 InstGetSystemFirmwareTable::imgCallId = 0;
CallSequenceNumber InstGetSystemFirmwareTable::fcnCallId = 0;
Notifier* InstGetSystemFirmwareTable::globalNotifierPtr;

/* =========================================================================================
   Sanitização RSMB (SMBIOS raw) – evita detecção de VM
   - Faz replace in-place de substrings “conhecidas de VM” mantendo o MESMO tamanho
   - Só aplica quando provider == 'RSMB' (0x52534D42)
   - Loga cada patch (needle -> repl padded) e a contagem de trocas
   ========================================================================================= */

static inline std::string pad_or_trunc(const std::string& s, size_t len)
{
    if (s.size() == len) return s;
    if (s.size() < len)
    {
        std::string out = s;
        out.append(len - s.size(), ' ');
        return out;
    }
    return s.substr(0, len);
}

// busca ingênua por sequência bytes, exigindo '\0' logo após para minimizar falsos positivos
static size_t replace_cstring_exactlen_inplace(BYTE* buf, size_t buflen,
    const char* needle,
    const char* repl_same_or_less, // será padded
    std::stringstream& log)
{
    const size_t nlen = std::strlen(needle);
    if (!nlen || buflen < nlen) return 0;

    const std::string replPadded = pad_or_trunc(repl_same_or_less, nlen);
    size_t patches = 0;

    for (size_t i = 0; i + nlen < buflen; ++i)
    {
        // tem que casar needle e o byte seguinte precisa ser '\0' (provável fim de string SMBIOS)
        if (std::memcmp(buf + i, needle, nlen) == 0 && buf[i + nlen] == '\0')
        {
            // aplica patch com PIN_SafeCopy (escrita segura)
            size_t wrote = PIN_SafeCopy(buf + i, replPadded.data(), nlen);
            if (wrote == nlen)
            {
                patches++;
            }
        }
    }

    if (patches)
    {
        log << "    [Countermeasure/RSMB] \"" << needle << "\" -> \"" << pad_or_trunc(repl_same_or_less, nlen)
            << "\" (kept len " << nlen << ") x" << patches << "\n";
    }
    return patches;
}

static size_t sanitize_rsmblob_inplace(void* p, DWORD cb, std::stringstream& log)
{
    if (!p || cb == 0) return 0;
    BYTE* buf = reinterpret_cast<BYTE*>(p);
    size_t total = 0;

    // Mapeamentos simples (mantendo o mesmo tamanho)
    //  - BIOS Vendor: "innotek GmbH" (12) -> "PhoenixBIOS " (11 + espaço)
    total += replace_cstring_exactlen_inplace(buf, cb, "innotek GmbH", "PhoenixBIOS", log);

    //  - BIOS Version: "VirtualBox" (10) -> "Dell BIOS " (9 + espaço)
    total += replace_cstring_exactlen_inplace(buf, cb, "VirtualBox", "Dell BIOS", log);

    //  - BIOS Date: "12/01/2006" (10) -> "06/23/2017" (10)
    total += replace_cstring_exactlen_inplace(buf, cb, "12/01/2006", "06/23/2017", log);

    //  - Fabricantes/placas comuns em VMs
    total += replace_cstring_exactlen_inplace(buf, cb, "VMware", "DELL  ", log);    // (6) -> (6)
    total += replace_cstring_exactlen_inplace(buf, cb, "VBOX", "DELL", log);     // (4) -> (4)
    total += replace_cstring_exactlen_inplace(buf, cb, "QEMU", "ASUS", log);     // (4) -> (4)

    // (Opcional) outros indícios comuns
    total += replace_cstring_exactlen_inplace(buf, cb, "Virtual Machine", "Workstation    ", log); // (15)->(15), 4 espaços
    total += replace_cstring_exactlen_inplace(buf, cb, "Virtual", "Genuine", log); // (7)->(7)

    return total;
}

/* ========================================================================================= */

VOID InstGetSystemFirmwareTable::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT FirmwareTableProviderSignature, ADDRINT FirmwareTableID, ADDRINT pFirmwareTableBuffer, ADDRINT BufferSize)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    auto* args = new GetSystemFirmwareTableArgs;
    args->FirmwareTableProviderSignature = FirmwareTableProviderSignature;
    args->FirmwareTableID = FirmwareTableID;
    args->pFirmwareTableBuffer = pFirmwareTableBuffer;
    args->BufferSize = BufferSize;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& ss = callContext->stringStream;
    ss << std::endl << "[+] GetSystemFirmwareTable..." << std::endl;
    ss << "    Thread: " << tid << std::endl;
    ss << "    Call Id: " << fcnCallId << std::endl;
    ss << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    ss << "    Parameters: " << std::endl;
    ss << "        FirmwareTableProviderSignature: 0x" << std::hex << FirmwareTableProviderSignature << std::dec << std::endl;
    ss << "        FirmwareTableID: 0x" << std::hex << FirmwareTableID << std::dec << std::endl;
    ss << "        pFirmwareTableBuffer: 0x" << std::hex << pFirmwareTableBuffer << std::dec << std::endl;
    ss << "        BufferSize: " << BufferSize << " bytes" << std::endl;
    ss << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    ss << "  [-] Start of call GetSystemFirmwareTable" << std::endl;
}

VOID InstGetSystemFirmwareTable::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT FirmwareTableProviderSignature, ADDRINT FirmwareTableID, ADDRINT pFirmwareTableBuffer, ADDRINT BufferSize)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr)
    {
        CallContext* callContext = pendingCallContext;
        std::stringstream& ss = callContext->stringStream;
        GetSystemFirmwareTableArgs* args = static_cast<GetSystemFirmwareTableArgs*>(callContext->functionArgs);

        DWORD bytesCopied = static_cast<DWORD>(retValue);
        ss << "    Returned from GetSystemFirmwareTable: " << bytesCopied << " bytes" << std::endl;

        // Se retornou dados E foi pedido provider RSMB, sanitiza in-place
        const DWORD RSMB = 0x52534D42u; // 'R''S''M''B' (little-endian)

        if (bytesCopied != 0)
        {

            if (args->FirmwareTableProviderSignature == RSMB && args->pFirmwareTableBuffer > 1000)
            {
                // tamanho efetivo = min(BufferSize, bytesCopied) por segurança
                DWORD cbEff = args->BufferSize ? std::min<DWORD>(args->BufferSize, bytesCopied) : bytesCopied;

                size_t nPatches = sanitize_rsmblob_inplace(reinterpret_cast<void*>(args->pFirmwareTableBuffer), cbEff, ss);
                if (nPatches == 0)
                    ss << "    [Countermeasure/RSMB] No VM strings found to patch.\n";
                else
                    ss << "    [Countermeasure/RSMB] Total patches: " << nPatches << "\n";
            }

            // Dump limitado do início do buffer (para depuração)
            if (args->pFirmwareTableBuffer != 0 && args->BufferSize > 0)
            {
                DWORD bytesToDisplay = (std::min<DWORD>(args->BufferSize, bytesCopied) < 64) ?
                    std::min<DWORD>(args->BufferSize, bytesCopied) : 64;

                std::vector<BYTE> firmwareTableData(bytesToDisplay);
                if (!firmwareTableData.empty())
                {
                    PIN_SafeCopy(&firmwareTableData[0],
                        reinterpret_cast<const VOID*>(args->pFirmwareTableBuffer),
                        bytesToDisplay);

                    ss << "    Data from FirmwareTableBuffer (" << bytesToDisplay << " bytes): ";
                    for (DWORD i = 0; i < bytesToDisplay; ++i)
                        ss << std::hex << (int)firmwareTableData[i] << " ";
                    ss << std::dec << "\n";
                }
            }

            ss << "    Operation completed successfully." << std::endl;
        }
        else
        {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            ss << "    Failed to get firmware table. Error code: " << error << std::endl;
        }

        ss << "  [-] Call from GetSystemFirmwareTable completed" << std::endl;
        ss << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation info = { ss.str() };
        ExecutionEventData ev(info);
        globalNotifierPtr->NotifyAll(&ev);

        delete reinterpret_cast<GetSystemFirmwareTableArgs*>(callContext->functionArgs);
        delete callContext;
    }
}

VOID InstGetSystemFirmwareTable::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetSystemFirmwareTable")
    {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x11;
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
                        IARG_RETURN_IP,                        // Endere�o da fun��o chamante
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,         // FirmwareTableProviderSignature
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,         // FirmwareTableID
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,         // pFirmwareTableBuffer
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,         // BufferSize
                        IARG_END);
                }
            }
        }
        else {
            // DWORD GetSystemFirmwareTable(DWORD, DWORD, PVOID, DWORD)
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
                IARG_THREAD_ID,
                IARG_UINT32, imgCallId,
                IARG_INST_PTR,
                IARG_ADDRINT, RTN_Address(rtn),
                IARG_CONTEXT,
                IARG_RETURN_IP,                           // chamador
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,         // FirmwareTableProviderSignature
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,         // FirmwareTableID
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,         // pFirmwareTableBuffer
                IARG_FUNCARG_ENTRYPOINT_VALUE, 3,         // BufferSize
                IARG_END);
        }
        

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR,            // endereço de retorno (se precisar)
            IARG_FUNCRET_EXITPOINT_VALUE,             // DWORD ret
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,         // FirmwareTableProviderSignature
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,         // FirmwareTableID
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,         // pFirmwareTableBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,         // BufferSize
            IARG_END);

        RTN_Close(rtn);
    }
}
