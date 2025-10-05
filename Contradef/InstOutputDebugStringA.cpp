#include "InstOutputDebugStringA.h"

std::map<CallContextKey, CallContext*> InstOutputDebugStringA::callContextMap;
UINT32 InstOutputDebugStringA::imgCallId = 0;
UINT32 InstOutputDebugStringA::fcnCallId = 0;
Notifier* InstOutputDebugStringA::globalNotifierPtr;

VOID InstOutputDebugStringA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpOutputString) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    OutputDebugStringAArgs* args = new OutputDebugStringAArgs;
    args->lpOutputString = lpOutputString;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] OutputDebugStringA..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;

    // Tenta ler a string de saída
    if (lpOutputString != 0) {
        std::string debugStr;
        debugStr.resize(256);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&debugStr[0], reinterpret_cast<char*>(lpOutputString), 255);
        debugStr[charsRead] = '\0';
        stringStream << "        lpOutputString: " << debugStr << std::endl;
    }
    else {
        stringStream << "        lpOutputString: NULL" << std::endl;
    }

    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call OutputDebugStringA" << std::endl;

}

VOID InstOutputDebugStringA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT lpOutputString) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        // OutputDebugStringA é VOID, não há valor de retorno significativo
        // Apenas indicar conclusão
        stringStream << "    Operation completed (função VOID)." << std::endl;
        stringStream << "  [-] Call from OutputDebugStringA completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const OutputDebugStringAArgs* args = reinterpret_cast<OutputDebugStringAArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstOutputDebugStringA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "OutputDebugStringA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // VOID OutputDebugStringA(LPCSTR lpOutputString);

        // Inserção do CallbackBefore
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
            IARG_END);

        // Inserção do CallbackAfter (função VOID, mas ainda podemos registrar a finalização)
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
            IARG_END);

        RTN_Close(rtn);
    }
}
