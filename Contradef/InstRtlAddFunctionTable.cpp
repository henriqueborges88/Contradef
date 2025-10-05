#include "InstRtlAddFunctionTable.h"

std::map<CallContextKey, CallContext*> InstRtlAddFunctionTable::callContextMap;
UINT32 InstRtlAddFunctionTable::imgCallId = 0;
UINT32 InstRtlAddFunctionTable::fcnCallId = 0;
Notifier* InstRtlAddFunctionTable::globalNotifierPtr;

VOID InstRtlAddFunctionTable::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT FunctionTable, ADDRINT EntryCount, ADDRINT BaseAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RtlAddFunctionTableArgs* args = new RtlAddFunctionTableArgs;
    args->FunctionTable = FunctionTable;
    args->EntryCount = EntryCount;
    args->BaseAddress = BaseAddress;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] RtlAddFunctionTable..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        FunctionTable: 0x" << std::hex << FunctionTable << std::dec << std::endl;
    stringStream << "        EntryCount: " << EntryCount << std::endl;
    stringStream << "        BaseAddress: 0x" << std::hex << BaseAddress << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call RtlAddFunctionTable" << std::endl;

}

VOID InstRtlAddFunctionTable::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT FunctionTable, ADDRINT EntryCount, ADDRINT BaseAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from RtlAddFunctionTable: " << result << std::endl;

        if (result != 0) {
            stringStream << "    Function table added successfully." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to add function table. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from RtlAddFunctionTable completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const RtlAddFunctionTableArgs* args = reinterpret_cast<RtlAddFunctionTableArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstRtlAddFunctionTable::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RtlAddFunctionTable") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura da RtlAddFunctionTable (Win32/64):
        // BOOLEAN RtlAddFunctionTable(
        //   PRUNTIME_FUNCTION FunctionTable,
        //   DWORD EntryCount,
        //   DWORD64 BaseAddress
        // );
        // Vale notar que FunctionTable é um ponteiro, EntryCount é DWORD e BaseAddress é um DWORD64.

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // FunctionTable
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // EntryCount
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // BaseAddress
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (BOOLEAN)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // FunctionTable
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // EntryCount
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // BaseAddress
            IARG_END);

        RTN_Close(rtn);
    }
}
