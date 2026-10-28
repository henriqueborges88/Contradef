#include "InstRtlDeleteFunctionTable.h"

CallContextMap InstRtlDeleteFunctionTable::callContextMap;
UINT32 InstRtlDeleteFunctionTable::imgCallId = 0;
CallSequenceNumber InstRtlDeleteFunctionTable::fcnCallId = 0;
Notifier* InstRtlDeleteFunctionTable::globalNotifierPtr;

VOID InstRtlDeleteFunctionTable::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT FunctionTable) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    RtlDeleteFunctionTableArgs* args = new RtlDeleteFunctionTableArgs;
    args->FunctionTable = FunctionTable;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] RtlDeleteFunctionTable..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        FunctionTable: " << std::hex << FunctionTable << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call RtlDeleteFunctionTable" << std::endl;

}

VOID InstRtlDeleteFunctionTable::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        if (retVal != 0) {
            stringStream << "    Function table deleted successfully." << std::endl;
        }
        else {
            stringStream << "    Failed to delete function table." << std::endl;
        }
        stringStream << "  [-] Call from RtlDeleteFunctionTable completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const RtlDeleteFunctionTableArgs* args = reinterpret_cast<RtlDeleteFunctionTableArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstRtlDeleteFunctionTable::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "RtlDeleteFunctionTable") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0xf;
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
                        IARG_RETURN_IP,                        // Endereço da função chamante
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // FunctionTable
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
                IARG_RETURN_IP,                        // Endereço da função chamante
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // FunctionTable
                IARG_END);
        }


        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno
            IARG_END);

        RTN_Close(rtn);
    }
}
