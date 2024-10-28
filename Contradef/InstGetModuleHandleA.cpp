/*
Para o Themida:
------> kernel32.dll
------> gdi32.dll
------> ole32.dll
------> oleaut32.dll
------> dateinj01.dll -> Pesquisar mais -> https://www.exedb.com/md/dateinj01---b67eb0f402c1ab9d6b5e9bcce61f80fb.shtml
------> cmdvrt32.dll -> Relacionado com COMODO Internet Security -> https://www.freefixer.com/library/file/cmdvrt32.dll-143267/  https://www.techtudo.com.br/tudo-sobre/comodo-internet-security/
------> SbieDll.dll -> Relacionado com sandboxie -> https://sandboxie-website-archive.github.io/www.sandboxie.com/index.html
------> KERNEL32.DLL
*/
#include "InstGetModuleHandleA.h"

std::map<CallContextKey, CallContext*> InstGetModuleHandleA::callContextMap;
UINT32 InstGetModuleHandleA::imgCallId = 0;
UINT32 InstGetModuleHandleA::fcnCallId = 0;
Notifier* InstGetModuleHandleA::globalNotifierPtr;


VOID InstGetModuleHandleA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpModuleName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    GetModuleHandleAArgs args;
    args.lpModuleName = lpModuleName;

    //// CONTRAMEDIDA
    args.originalLastChar = *reinterpret_cast<char*>(lpModuleName + strlen(reinterpret_cast<char*>(lpModuleName)) - 1); // Salvar o �ltimo caractere original de lpSubKey

    std::string ansiStringLpModuleName = ConvertAddrToAnsiString(lpModuleName);
    if (isModulePartInList(ansiStringLpModuleName)) {
        char* ansiCharStr = reinterpret_cast<char*>(lpModuleName);
        size_t len = strlen(ansiCharStr);

        if (len > 0) {
            ansiCharStr[len - 1] = 'q';
            args.isModified = true;
        }
    }
    ////

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);
    
    
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    }

VOID InstGetModuleHandleA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT lpModuleName) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar Parámetros
        const GetModuleHandleAArgs* args = reinterpret_cast<GetModuleHandleAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpModuleName = ConvertAddrToAnsiString(args->lpModuleName);

        ////CONTRAMEDIDA APLICADA - TECNICA EVASIVA DETECTADA
        if (args->isModified) {
            char* ansiCharStr = reinterpret_cast<char*>(args->lpModuleName);
            size_t len = strlen(ansiCharStr);

            if (len > 0) {
                ansiCharStr[len - 1] = args->originalLastChar;
            }

            // Obter a RTN da instru��o atual
            RTN rtnCurrent = RTN_FindByAddress(instAddress);
            stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
            stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
            stringStream << "    Thread: " << tid << std::endl;
            stringStream << "    Id de chamada: " << fcnCallId << std::endl;
            stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
            stringStream << "    Parámetros: " << std::endl;
            stringStream << "        lpModuleName: " << ansiStringLpModuleName << std::endl;
            stringStream << "    Valor de retorno: " << *retValAddr << std::endl;
            stringStream << "[*] Concluído" << std::endl << std::endl;

            ExecutionInformation executionCompletedInfo = { stringStream.str() };
            // Cria evento
            ExecutionEventData executionEvent(executionCompletedInfo);
            // Notifica os observers
            globalNotifierPtr->NotifyAll(&executionEvent);
        }
        ////

        delete callContext;
        PIN_UnlockClient();
    }

    fcnCallId++;
}

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    SEC sec = RTN_Sec(rtn);
    IMG img = SEC_Img(sec);
    if (IMG_Valid(img)) {
        std::string imageName = IMG_Name(img);
        std::string moduleName = ExtractModuleName(IMG_Name(img));
        if (toUpperCase(moduleName) != "KERNELBASE.DLL") {
            return;
        }
    }
    else {
        return;
    }

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetModuleHandleA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);
        RTN_Close(rtn);
    }

}

