#include "InstPostQuitMessage.h"

CallContextMap InstPostQuitMessage::callContextMap;
UINT32 InstPostQuitMessage::imgCallId = 0;
CallSequenceNumber InstPostQuitMessage::fcnCallId = 0;
Notifier* InstPostQuitMessage::globalNotifierPtr = nullptr;

VOID InstPostQuitMessage::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT returnAddress,
    ADDRINT nExitCode)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    std::stringstream ss;

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    ss << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    ss << "    Module name: " << IMG_Name(img) << std::endl;
    ss << "    Thread: " << tid << std::endl;
    ss << "    Call Id: " << fcnCallId << std::endl;
    ss << "    Routine address: " << std::hex << rtnAddress << std::dec << std::endl;

    ss << "    Parameters: " << std::endl;
    ss << "        nExitCode: " << static_cast<INT32>(nExitCode) << std::endl;

    // Função VOID
    ss << "    Return: (void)" << std::endl;
    ss << "[*] Completed" << std::endl << std::endl;

    ExecutionInformation info = { ss.str() };
    ExecutionEventData evt(info);
    globalNotifierPtr->NotifyAll(&evt);

    //Fallback para problema de click no splash do temida. Reativando a instrumentação de memória e instruções
    if (IsStringInStrategyScope("Themida")) {
        if (KnobTraceInstructions) {
            TraceInstructions::enable();
        }
        if (KnobTraceMemory) {
            TraceMemory::enable();
        }
    }
    // Fim fallback
}


VOID InstPostQuitMessage::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "PostQuitMessage") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // VOID PostQuitMessage(int nExitCode);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                   // endereço do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // nExitCode
            IARG_END);
        RTN_Close(rtn);

    }

}
