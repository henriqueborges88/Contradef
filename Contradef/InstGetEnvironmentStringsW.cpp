#include "InstGetEnvironmentStringsW.h"

CallContextMap InstGetEnvironmentStringsW::callContextMap;
UINT32 InstGetEnvironmentStringsW::imgCallId = 0;
CallSequenceNumber InstGetEnvironmentStringsW::fcnCallId = 0;
Notifier* InstGetEnvironmentStringsW::globalNotifierPtr;

VOID InstGetEnvironmentStringsW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, nullptr);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetEnvironmentStringsW..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetEnvironmentStringsW" << std::endl;

}

VOID InstGetEnvironmentStringsW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

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
            using namespace WindowsAPI;
            // Converte o bloco de strings de ambiente para uma representação legível
            LPWSTR envStrings = reinterpret_cast<LPWSTR>(retVal);
            std::wstring allEnvStrings;
            LPWSTR current = envStrings;

            while (*current) {
                std::wstring envVar(current);
                allEnvStrings += envVar + L"\n";
                current += envVar.length() + 1;
            }

            // Converter std::wstring para std::string (considerando possível perda de informação)
            std::string envStringsStr(allEnvStrings.begin(), allEnvStrings.end());

            stringStream << "    Environment strings successfully obtained." << std::endl;
            stringStream << "    Environment:" << std::endl << envStringsStr << std::endl;
        }
        else {
            stringStream << "    Failed to get environment strings." << std::endl;
        }

        stringStream << "  [-] Call from GetEnvironmentStringsW completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetEnvironmentStringsW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetEnvironmentStringsW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                        // Endereço da função chamante
            IARG_END);

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
