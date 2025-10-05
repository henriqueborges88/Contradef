#include "InstGetFileSizeEx.h"

// Inicialização das variáveis estáticas
std::map<CallContextKey, CallContext*> InstGetFileSizeEx::callContextMap;
UINT32 InstGetFileSizeEx::imgCallId = 0;
UINT32 InstGetFileSizeEx::fcnCallId = 0;
Notifier* InstGetFileSizeEx::globalNotifierPtr;

// Callback executado antes da chamada da função GetFileSizeEx
VOID InstGetFileSizeEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hFile, ADDRINT lpFileSize) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    // Estrutura para armazenar os argumentos
    GetFileSizeExArgs* args = new GetFileSizeExArgs;
    args->hFile = hFile;
    args->lpFileSize = lpFileSize;

    // Criação de um ID único para o contexto da chamada
    UINT32 callCtxId = callId * 100 + fcnCallId;

    // Criação e armazenamento do contexto da chamada
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Construção da mensagem de log
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetFileSizeEx..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hFile: 0x" << std::hex << hFile << std::dec << std::endl;
    stringStream << "        lpFileSize: 0x" << std::hex << lpFileSize << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetFileSizeEx" << std::endl;

}

// Callback executado após a chamada da função GetFileSizeEx
VOID InstGetFileSizeEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT hFile, ADDRINT lpFileSize) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Criação do ID único para o contexto da chamada
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };

    // Busca do contexto da chamada no mapa
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {

        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        // Conversão do valor de retorno para BOOL
        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from GetFileSizeEx: " << result << std::endl;

        if (result != 0) {
            // Sucesso: tenta ler o valor retornado em lpFileSize
            using namespace WindowsAPI;
            LARGE_INTEGER fileSize;
            if (lpFileSize != 0) {
                PIN_SafeCopy(&fileSize, reinterpret_cast<LARGE_INTEGER*>(lpFileSize), sizeof(LARGE_INTEGER));
                stringStream << "    File size: " << fileSize.QuadPart << " bytes" << std::endl;
            }
            else {
                stringStream << "    lpFileSize is NULL. Could not read file size." << std::endl;
            }
            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            // Falha: obtém o código de erro
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to get file size. Error code: " << error << std::endl;
        }

        // Finalização das mensagens de log
        stringStream << "  [-] Call from GetFileSizeEx completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        // Notificação aos observadores após capturar o resultado
        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        // Limpeza: remove e deleta o contexto da chamada
        delete reinterpret_cast<GetFileSizeExArgs*>(callContext->functionArgs);
        delete callContext;
        callContextMap.erase(it);

    }

    // Incrementa o ID da chamada
    fcnCallId++;
}

// Método responsável por inserir os callbacks na função GetFileSizeEx
VOID InstGetFileSizeEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetFileSizeEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // BOOL GetFileSizeEx(
        //   HANDLE         hFile,
        //   PLARGE_INTEGER lpFileSize
        // );

        // Inserção do CallbackBefore antes da chamada da função
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFileSize
            IARG_END);

        // Inserção do CallbackAfter após a chamada da função
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,     // valor de retorno (BOOL)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpFileSize
            IARG_END);

        RTN_Close(rtn);
    }
}
