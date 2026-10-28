#include "InstSetEvent.h"

CallContextMap InstSetEvent::callContextMap;
UINT32 InstSetEvent::imgCallId = 0;
CallSequenceNumber InstSetEvent::fcnCallId = 0;
Notifier* InstSetEvent::globalNotifierPtr;

VOID InstSetEvent::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hEvent) {

    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            return;
        }
    }

    SetEventArgs* args = new SetEventArgs;
    args->hEvent = hEvent;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] SetEvent..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hEvent: " << std::hex << hEvent << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call SetEvent" << std::endl;

}

VOID InstSetEvent::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
            return;
        }
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        if (retVal != 0) {
            stringStream << "    Event successfully flagged." << std::endl;
        }
        else {
            stringStream << "    Failed to signal event." << std::endl;
        }
        stringStream << "  [-] Call from SetEvent completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const SetEventArgs* args = reinterpret_cast<SetEventArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstSetEvent::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "SetEvent") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x12;
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
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hEvent
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
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hEvent
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
