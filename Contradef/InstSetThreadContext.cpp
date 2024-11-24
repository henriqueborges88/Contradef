#include "InstSetThreadContext.h"
#include "NtStructures.h"

std::map<CallContextKey, CallContext*> InstSetThreadContext::callContextMap;
UINT32 InstSetThreadContext::imgCallId = 0;
UINT32 InstSetThreadContext::fcnCallId = 0;
Notifier* InstSetThreadContext::globalNotifierPtr;

VOID InstSetThreadContext::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hThread, ADDRINT lpContext) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    SetThreadContextArgs args;
    args.hThread = hThread;
    args.lpContext = lpContext;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, &args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream stringStream;
    stringStream << std::endl << "[+] SetThreadContext..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    ID de chamada: " << fcnCallId << std::endl;
    stringStream << "    Endereço da rotina: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parâmetros: " << std::endl;
    stringStream << "        hThread: " << std::hex << hThread << std::dec << std::endl;
    stringStream << "        lpContext: " << std::hex << lpContext << std::dec << std::endl;

    if (lpContext != 0) {
        ::WindowsAPI::CONTEXT ctxData;
        PIN_SafeCopy(&ctxData, reinterpret_cast<void*>(lpContext), sizeof(::WindowsAPI::CONTEXT));

        stringStream << "    Contexto da Thread para Configuração:" << std::endl;
        stringStream << "        Rax: " << std::hex << ctxData.Rax << std::dec << std::endl;
        stringStream << "        Rbx: " << std::hex << ctxData.Rbx << std::dec << std::endl;
        stringStream << "        Rcx: " << std::hex << ctxData.Rcx << std::dec << std::endl;
        stringStream << "        Rdx: " << std::hex << ctxData.Rdx << std::dec << std::endl;
        // Outros registros podem ser adicionados aqui
    }

    stringStream << "    Endereço da função chamante: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "[*] Início da chamada SetThreadContext" << std::endl << std::endl;

    ExecutionInformation executionCompletedInfo = { stringStream.str() };
    ExecutionEventData executionEvent(executionCompletedInfo);
    globalNotifierPtr->NotifyAll(&executionEvent);
}

VOID InstSetThreadContext::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT retVal) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Valor de retorno: " << retVal << std::endl;

        if (retVal != 0) {
            stringStream << "    Configuração do contexto da thread bem-sucedida." << std::endl;
        }
        else {
            stringStream << "    Falha ao configurar o contexto da thread." << std::endl;
        }

        stringStream << "[*] Chamada SetThreadContext concluída" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
        callContextMap.erase(it);
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstSetThreadContext::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "SetThreadContext") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hThread
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // lpContext
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                        // Endereço da função chamante
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno
            IARG_END);

        RTN_Close(rtn);
    }
}
