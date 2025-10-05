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

    GetModuleHandleAArgs* args = new GetModuleHandleAArgs;
    args->lpModuleName = lpModuleName;

    std::string ansiStringLpModuleName = ConvertAddrToAnsiString(lpModuleName);


    //// CONTRAMEDIDA
    //args.originalLastChar = *reinterpret_cast<char*>(lpModuleName + strlen(reinterpret_cast<char*>(lpModuleName)) - 1); // Salvar o �ltimo caractere original de lpSubKey

    //if (isModulePartInList(ansiStringLpModuleName)) {
    //    char* ansiCharStr = reinterpret_cast<char*>(lpModuleName);
    //    size_t len = strlen(ansiCharStr);

    //    if (len > 0) {
    //        ansiCharStr[len - 1] = 'q';
    //        args.isModified = true;
    //    }
    //}
    ////

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    
    
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os parâmetros de GetModuleHandleA e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetModuleHandleA..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpModuleName: " << (!ansiStringLpModuleName.empty() ? ansiStringLpModuleName : "NULL (Main Executable)") << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of module consultation" << std::endl;

}

VOID InstGetModuleHandleA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress, ADDRINT lpModuleName) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun�ao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        // Registrar Parámetros
        const GetModuleHandleAArgs* args = reinterpret_cast<GetModuleHandleAArgs*>(callContext->functionArgs);

        std::string ansiStringLpModuleName = ConvertAddrToAnsiString(lpModuleName);

        ////CONTRAMEDIDA APLICADA - TECNICA EVASIVA DETECTADA
        //if (args->isModified) {
        //    size_t len = strlen(ansiStringLpModuleName.c_str());

        //    if (len > 0) {
        //        ansiStringLpModuleName[len - 1] = args->originalLastChar;
        //    }
        //}
        ////


        
        std::stringstream& stringStream = callContext->stringStream;
        stringStream << "    Returned module handle: " << std::hex << retValue << std::dec << std::endl;
        stringStream << "  [-] Operation completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;


        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetModuleHandleAArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    //SEC sec = RTN_Sec(rtn);
    //IMG img = SEC_Img(sec);
    //if (IMG_Valid(img)) {
    //    std::string imageName = IMG_Name(img);
    //    std::string moduleName = ExtractModuleName(IMG_Name(img));
    //    if (toUpperCase(moduleName) != "KERNELBASE.DLL") {
    //        return;
    //    }
    //}
    //else {
    //    return;
    //}

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
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);
        RTN_Close(rtn);
    }

}

