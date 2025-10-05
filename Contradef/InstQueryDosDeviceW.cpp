#include "InstQueryDosDeviceW.h"

// Inicialização das variáveis estáticas
std::map<CallContextKey, CallContext*> InstQueryDosDeviceW::callContextMap;
UINT32 InstQueryDosDeviceW::imgCallId = 0;
UINT32 InstQueryDosDeviceW::fcnCallId = 0;
Notifier* InstQueryDosDeviceW::globalNotifierPtr;

// Callback executado antes da chamada da função QueryDosDeviceW
VOID InstQueryDosDeviceW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpDeviceName, ADDRINT lpTargetPath, ADDRINT ucchMax) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    // Estrutura para armazenar os argumentos
    QueryDosDeviceWArgs* args = new QueryDosDeviceWArgs;
    args->lpDeviceName = lpDeviceName;
    args->lpTargetPath = lpTargetPath;
    args->ucchMax = ucchMax;

    // Criação de um ID único para o contexto da chamada
    UINT32 callCtxId = callId * 100 + fcnCallId;

    // Criação e armazenamento do contexto da chamada
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Construção da mensagem de log
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] QueryDosDeviceW..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpDeviceName: 0x" << std::hex << lpDeviceName << std::dec;

    // Tenta ler o nome do dispositivo, se possível
    if (lpDeviceName != 0) {
        std::wstring deviceName;
        deviceName.resize(256); // Assumindo um tamanho máximo
        PIN_SafeCopy(&deviceName[0], reinterpret_cast<wchar_t*>(lpDeviceName), sizeof(wchar_t) * 256);
        stringStream << " (" << WStringToString(deviceName.c_str()) << ")";
    }
    stringStream << std::endl;

    stringStream << "        lpTargetPath: 0x" << std::hex << lpTargetPath << std::dec << std::endl;
    stringStream << "        ucchMax: " << ucchMax << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call QueryDosDeviceW" << std::endl;

}

// Callback executado após a chamada da função QueryDosDeviceW
VOID InstQueryDosDeviceW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT lpDeviceName, ADDRINT lpTargetPath, ADDRINT ucchMax) {

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

        // Conversão do valor de retorno para DWORD
        DWORD result = static_cast<DWORD>(retValue);
        stringStream << "    Returned from QueryDosDeviceW: " << result << std::endl;

        if (result != 0) {
            // Sucesso: tenta ler o caminho de destino retornado em lpTargetPath
            if (lpTargetPath != 0) {
                std::wstring targetPath;
                targetPath.resize(ucchMax);
                PIN_SafeCopy(&targetPath[0], reinterpret_cast<wchar_t*>(lpTargetPath), sizeof(wchar_t) * ucchMax);
                stringStream << "    Destination path: " << WStringToString(targetPath.c_str()) << std::endl;
            }
            else {
                stringStream << "    lpTargetPath is NULL. Could not read the target path." << std::endl;
            }
            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            // Falha: obtém o código de erro
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to query device. Error code: " << error << std::endl;
        }

        // Finalização das mensagens de log
        stringStream << "  [-] Call from QueryDosDeviceW completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        // Notificação aos observadores após capturar o resultado
        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const QueryDosDeviceWArgs* args = reinterpret_cast<QueryDosDeviceWArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);

    }

    // Incrementa o ID da chamada
    fcnCallId++;
}

// Método responsável por inserir os callbacks na função QueryDosDeviceW
VOID InstQueryDosDeviceW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "QueryDosDeviceW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // DWORD QueryDosDeviceW(
        //   LPCWSTR lpDeviceName,
        //   LPWSTR  lpTargetPath,
        //   DWORD   ucchMax
        // );

        // Inserção do CallbackBefore antes da chamada da função
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpDeviceName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpTargetPath
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ucchMax
            IARG_END);

        // Inserção do CallbackAfter após a chamada da função
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (DWORD)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpDeviceName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpTargetPath
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ucchMax
            IARG_END);

        RTN_Close(rtn);
    }
}
