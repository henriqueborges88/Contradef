#include "InstGetCommandLineA.h"

std::map<CallContextKey, CallContext*> InstGetCommandLineA::callContextMap;
UINT32 InstGetCommandLineA::imgCallId = 0;
UINT32 InstGetCommandLineA::fcnCallId = 0;
Notifier* InstGetCommandLineA::globalNotifierPtr;

VOID InstGetCommandLineA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, nullptr);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetCommandLineA..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: (none)" << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetCommandLineA" << std::endl;

}

VOID InstGetCommandLineA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retValue) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        // Valor de retorno é um LPSTR (char*), se não for NULL, podemos tentar ler a string
        char* cmdLine = reinterpret_cast<char*>(retValue);
        if (cmdLine != nullptr) {
            // Tenta ler a string até um limite para evitar logs excessivos
            std::string cmdLineStr;
            cmdLineStr.resize(256);
            WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&cmdLineStr[0], cmdLine, 255);
            // Garante terminação
            cmdLineStr[charsRead] = '\0';

            stringStream << "    Returned from GetCommandLineA: 0x" << std::hex << retValue << std::dec << " ("
                << cmdLineStr << ")" << std::endl;
        }
        else {
            stringStream << "    Returned from GetCommandLineA: NULL" << std::endl;
        }

        // Não há erro diretamente associado a esta função, mas se desejado pode-se consultar GetLastError().
        // Se não houver sentido, apenas indica conclusão.
        stringStream << "    Complete operation." << std::endl;
        stringStream << "  [-] Call from GetCommandLineA completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstGetCommandLineA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetCommandLineA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // LPSTR GetCommandLineA(void);
        //
        // Retorna um LPSTR (char* para a linha de comando atual do processo).

        // Inserção do CallbackBefore antes da chamada da função
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_END);

        // Inserção do CallbackAfter após a chamada da função
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (LPSTR)
            IARG_END);

        RTN_Close(rtn);
    }
}
