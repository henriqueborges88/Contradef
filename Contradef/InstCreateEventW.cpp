#include "InstCreateEventW.h"
#include "HeapTracker.h"
#include "SectionTracker.h"

std::map<CallContextKey, CallContext*> InstCreateEventW::callContextMap;
UINT32 InstCreateEventW::imgCallId = 0;
UINT32 InstCreateEventW::fcnCallId = 0;
Notifier* InstCreateEventW::globalNotifierPtr;

VOID InstCreateEventW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpEventAttributes, ADDRINT bManualReset, ADDRINT bInitialState, ADDRINT lpName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    CreateEventWArgs* args = new CreateEventWArgs;
    args->lpEventAttributes = lpEventAttributes;
    args->bManualReset = bManualReset;
    args->bInitialState = bInitialState;
    args->lpName = lpName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    std::wstring wsName = lpName ? ConvertAddrToWideString(lpName) : L"(null)";
    stringStream << std::endl << "[+] CreateEventW..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpEventAttributes: " << std::hex << lpEventAttributes << std::dec << std::endl;
    stringStream << "        bManualReset: " << bManualReset << std::endl;
    stringStream << "        bInitialState: " << bInitialState << std::endl;
    stringStream << "        lpName: " << WStringToString(wsName) << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call CreateEventW" << std::endl;

}

VOID InstCreateEventW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Handle do evento criado: " << std::hex << retVal << std::dec << std::endl;
        stringStream << "  [-] Call from CreateEventW completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const CreateEventWArgs* args = reinterpret_cast<CreateEventWArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstCreateEventW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateEventW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x14;
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
                            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // lpEventAttributes
                            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // bManualReset
                            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // bInitialState
                            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // lpName
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
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // lpEventAttributes
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // bManualReset
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // bInitialState
                IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // lpName
                IARG_END);
        }

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno (HANDLE)
            IARG_END);

        RTN_Close(rtn);
    }
}
