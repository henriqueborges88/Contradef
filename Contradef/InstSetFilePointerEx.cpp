#include "InstSetFilePointerEx.h"

// Inicialização das variáveis estáticas
CallContextMap InstSetFilePointerEx::callContextMap;
UINT32 InstSetFilePointerEx::imgCallId = 0;
CallSequenceNumber InstSetFilePointerEx::fcnCallId = 0;
Notifier* InstSetFilePointerEx::globalNotifierPtr;

// Callback executado antes da chamada da função SetFilePointerEx
VOID InstSetFilePointerEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hFile, ADDRINT liDistanceToMovePtr, ADDRINT lpNewFilePointer, ADDRINT dwMoveMethod) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }


    // Estrutura para armazenar os argumentos
    SetFilePointer::SetFilePointerExArgs args;
    args.hFile = hFile;
    // Tenta copiar a estrutura LARGE_INTEGER apontada por liDistanceToMovePtr
    if (liDistanceToMovePtr != 0) {
        PIN_SafeCopy(&args.liDistanceToMove, reinterpret_cast<SetFilePointer::LARGE_INTEGER*>(liDistanceToMovePtr), sizeof(SetFilePointer::LARGE_INTEGER));
    }
    else {
        // Se o ponteiro for NULL, inicializa com zero
        args.liDistanceToMove.QuadPart = 0;
    }
    args.lpNewFilePointer = lpNewFilePointer;
    args.dwMoveMethod = dwMoveMethod;

    // Criação de um ID único para o contexto da chamada
    UINT32 callCtxId = callId * 100 + fcnCallId;

    // Criação e armazenamento do contexto da chamada
    auto* callContext = new CallContext(callCtxId, tid, instAddress, &args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Construção da mensagem de log
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] SetFilePointerEx..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hFile: 0x" << std::hex << hFile << std::dec << std::endl;
    stringStream << "        liDistanceToMove: " << args.liDistanceToMove.QuadPart << " bytes" << std::endl;
    stringStream << "        lpNewFilePointer: 0x" << std::hex << lpNewFilePointer << std::dec << std::endl;
    stringStream << "        dwMoveMethod: " << dwMoveMethod << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call SetFilePointerEx" << std::endl;

}

// Callback executado após a chamada da função SetFilePointerEx
VOID InstSetFilePointerEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT hFile, ADDRINT liDistanceToMovePtr, ADDRINT lpNewFilePointer, ADDRINT dwMoveMethod) {

    // Verificação para instrumentar apenas o Main Executable, se aplicável
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Criação do ID único para o contexto da chamada
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };

    // Busca do contexto da chamada no mapa
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {

        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        // Conversão do valor de retorno para BOOL
        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from SetFilePointerEx: " << result << std::endl;

        if (result != 0) {
            // Sucesso: tenta ler o valor retornado em lpNewFilePointer
            if (lpNewFilePointer != 0) {
                SetFilePointer::LARGE_INTEGER newFilePointer;
                PIN_SafeCopy(&newFilePointer, reinterpret_cast<SetFilePointer::LARGE_INTEGER*>(lpNewFilePointer), sizeof(SetFilePointer::LARGE_INTEGER));
                stringStream << "    New file pointer: " << newFilePointer.QuadPart << " bytes" << std::endl;
            }
            else {
                stringStream << "    lpNewFilePointer is NULL. Could not read the new file pointer." << std::endl;
            }
            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            // Falha: obtém o código de erro
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to move file pointer. Error code: " << error << std::endl;
        }

        // Finalização das mensagens de log
        stringStream << "  [-] Call from SetFilePointerEx completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        // Notificação aos observadores após capturar o resultado
        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        // Limpeza: remove e deleta o contexto da chamada
        delete callContext;

    }

    // Incrementa o ID da chamada
    fcnCallId++;
}

// Método responsável por inserir os callbacks na função SetFilePointerEx
VOID InstSetFilePointerEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "SetFilePointerEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // BOOL SetFilePointerEx(
        //   HANDLE         hFile,
        //   LARGE_INTEGER  liDistanceToMove,
        //   PLARGE_INTEGER lpNewFilePointer,
        //   DWORD          dwMoveMethod
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // liDistanceToMove (pointer)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpNewFilePointer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwMoveMethod
            IARG_END);

        // Inserção do CallbackAfter após a chamada da função
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (BOOL)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // liDistanceToMove (pointer)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpNewFilePointer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwMoveMethod
            IARG_END);

        RTN_Close(rtn);
    }
}
