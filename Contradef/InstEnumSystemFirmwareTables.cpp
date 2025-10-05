#include "InstEnumSystemFirmwareTables.h"

std::map<CallContextKey, CallContext*> InstEnumSystemFirmwareTables::callContextMap;
UINT32 InstEnumSystemFirmwareTables::imgCallId = 0;
UINT32 InstEnumSystemFirmwareTables::fcnCallId = 0;
Notifier* InstEnumSystemFirmwareTables::globalNotifierPtr = nullptr;

VOID InstEnumSystemFirmwareTables::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT*,
    ADDRINT returnAddress,
    ADDRINT FirmwareTableProviderSignature,
    ADDRINT pFirmwareTableEnumBuffer,
    ADDRINT BufferSize)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) return;

    EnumSystemFirmwareTablesArgs* args = new EnumSystemFirmwareTablesArgs{
        FirmwareTableProviderSignature,
        pFirmwareTableEnumBuffer,
        BufferSize
    };

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* ctx = new CallContext(callCtxId, tid, instAddress, args);
    callContextMap[{callCtxId, tid}] = ctx;

    std::stringstream& s = ctx->stringStream;
    s << "\n[+] EnumSystemFirmwareTables...\n"
        << "    Thread: " << tid << '\n'
        << "    Call Id: " << fcnCallId << '\n'
        << "    Routine address: 0x" << std::hex << ctx->rtnAddress << std::dec << '\n'
        << "    Parameters:\n"
        << "        FirmwareTableProviderSignature: 0x" << std::hex << FirmwareTableProviderSignature << std::dec << '\n'
        << "        pFirmwareTableEnumBuffer: 0x" << std::hex << pFirmwareTableEnumBuffer << std::dec << '\n'
        << "        BufferSize: " << BufferSize << " bytes\n"
        << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << '\n'
        << "  [-] Start of call EnumSystemFirmwareTables\n";

}

VOID InstEnumSystemFirmwareTables::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT*,
    ADDRINT* returnAddress,
    ADDRINT retValue,
    ADDRINT FirmwareTableProviderSignature,
    ADDRINT pFirmwareTableEnumBuffer,
    ADDRINT BufferSize)
{
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) return;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto it = callContextMap.find({ callCtxId, tid });
    if (it != callContextMap.end()) {
        CallContext* ctx = it->second;
        std::stringstream& s = ctx->stringStream;

        DWORD bytesReturned = static_cast<DWORD>(retValue);
        s << "    Returned from EnumSystemFirmwareTables (bytes needed/returned): "
            << bytesReturned << '\n';

        if (bytesReturned && pFirmwareTableEnumBuffer && BufferSize) {
            DWORD toShow = (bytesReturned < 128 ? bytesReturned : 128);
            std::vector<BYTE> data(toShow);
            WindowsAPI::SIZE_T copied = PIN_SafeCopy(data.begin(),
                reinterpret_cast<BYTE*>(pFirmwareTableEnumBuffer),
                toShow);
            s << "    First " << copied << " bytes of buffer: ";
            for (WindowsAPI::SIZE_T i = 0; i < copied; ++i) s << std::hex << (int)data[i] << ' ';
            s << std::dec << '\n';
        }

        s << "  [-] Call from EnumSystemFirmwareTables completed\n"
            << "[*] Completed\n\n";

        globalNotifierPtr->NotifyAll(new ExecutionEventData({ s.str() }));

        delete reinterpret_cast<EnumSystemFirmwareTablesArgs*>(ctx->functionArgs);
        delete ctx;
        callContextMap.erase(it);
    }
    fcnCallId++;
}

VOID InstEnumSystemFirmwareTables::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    if (RTN_Name(rtn) != "EnumSystemFirmwareTables") return;

    imgCallId++;
    globalNotifierPtr = &globalNotifier;
    RTN_Open(rtn);

    // Assinatura:
    // UINT EnumSystemFirmwareTables(
    //      DWORD  FirmwareTableProviderSignature,
    //      PVOID  pFirmwareTableEnumBuffer,
    //      DWORD  BufferSize);

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_RETURN_IP,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // FirmwareTableProviderSignature
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pFirmwareTableEnumBuffer
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // BufferSize
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
        IARG_THREAD_ID,
        IARG_UINT32, imgCallId,
        IARG_INST_PTR,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_CONTEXT,
        IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
        IARG_FUNCRET_EXITPOINT_VALUE,      // DWORD retornado
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,  // FirmwareTableProviderSignature
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,  // pFirmwareTableEnumBuffer
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,  // BufferSize
        IARG_END);

    RTN_Close(rtn);
}
