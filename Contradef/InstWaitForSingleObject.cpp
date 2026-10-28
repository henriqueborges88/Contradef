#include "InstWaitForSingleObject.h"


CallContextMap InstWaitForSingleObject::callContextMap;
UINT32 InstWaitForSingleObject::imgCallId = 0;
CallSequenceNumber InstWaitForSingleObject::fcnCallId = 0;
Notifier* InstWaitForSingleObject::globalNotifierPtr;

VOID InstWaitForSingleObject::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hHandle, ADDRINT dwMilliseconds) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    WaitForSingleObjectArgs* args = new WaitForSingleObjectArgs;
    args->hHandle = hHandle;
    args->dwMilliseconds = dwMilliseconds;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);
    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] WaitForSingleObject..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hHandle: " << std::hex << hHandle << std::dec << std::endl;
    stringStream << "        dwMilliseconds: ";
    if (dwMilliseconds == INFINITE) {
        stringStream << "INFINITE";
    }
    else {
        stringStream << dwMilliseconds << " ms";
    }
    stringStream << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "[*] Completed (WAITING RETURN)" << std::endl << std::endl;

    ExecutionInformation executionCompletedInfo = { stringStream.str() };
    ExecutionEventData executionEvent(executionCompletedInfo);
    globalNotifierPtr->NotifyAll(&executionEvent);
}

VOID InstWaitForSingleObject::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << std::endl << "[+] [RETURN] WaitForSingleObject..." << std::endl;
        stringStream << "    Return Value: ";
        switch (retVal) {
        case WAIT_OBJECT_0:
            stringStream << "WAIT_OBJECT_0";
            break;
        case WAIT_TIMEOUT:
            stringStream << "WAIT_TIMEOUT";
            break;
        case WAIT_FAILED:
            stringStream << "WAIT_FAILED";
            break;
        case WAIT_ABANDONED:
            stringStream << "WAIT_ABANDONED";
            break;
        default:
            stringStream << std::hex << retVal << std::dec;
        }
        stringStream << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const WaitForSingleObjectArgs* args = reinterpret_cast<WaitForSingleObjectArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstWaitForSingleObject::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "WaitForSingleObject") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);


        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x3;
            ADDRINT base = RTN_Address(rtn);
            for (INS i = RTN_InsHead(rtn);
                INS_Valid(i) && INS_Address(i) <= base + scanSize;
                i = INS_Next(i))
            {

                if (INS_Address(i) == base + scanSize)
                {
                    INS_InsertCall(i, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
                        IARG_THREAD_ID,
                        IARG_UINT32, imgCallId,
                        IARG_INST_PTR,
                        IARG_ADDRINT, RTN_Address(rtn),
                        IARG_CONTEXT,
                        IARG_RETURN_IP,                        // Endereço da função chamante
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hHandle
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // dwMilliseconds
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
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hHandle
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // dwMilliseconds
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
