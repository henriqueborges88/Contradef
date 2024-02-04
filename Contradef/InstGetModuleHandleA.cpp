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
std::ofstream* InstGetModuleHandleA::outStream;
Notifier* InstGetModuleHandleA::globalNotifierPtr;

VOID InstGetModuleHandleA::CheckPrintConditions(CallContext* callContext) {
    if (!(callContext->countInstructionsAfterCall >= 20 && callContext->countFunctionsCalledAfterCall >= 20)) {
        return;
    }
    
    std::stringstream& stringStream = callContext->stringStream;
    
    stringStream << "    Instruções executadas na rotina chamadora:" << std::endl;
    for (const auto& instructionInfo : callContext->instructionsAfterCall) {
        //criar metodo para marcar a instrução que chama a função
        stringStream << "        " << std::hex << instructionInfo.address << std::dec << " | " << instructionInfo.disassembledInstruction << std::endl;
    }
    
    stringStream << "    Pila de chamadas:" << std::endl;
    for (const auto& callFunctionInfo : callContext->functionsCalledAfterCall) {
        if (callFunctionInfo.address == callContext->rtnAddress) {
            stringStream << "        " << std::hex << callFunctionInfo.address << std::dec << " | --> " << callFunctionInfo.functionName << " (chamada por: " << callFunctionInfo.rtnCallerName << ")" << std::endl;
        }
        else {
            stringStream << "        " << std::hex << callFunctionInfo.address << std::dec << " |     " << callFunctionInfo.functionName << " (chamada por: " << callFunctionInfo.rtnCallerName << ")" << std::endl;
        }
    }
    
    stringStream << "[*] Concluído" << std::endl << std::endl;

    *outStream << stringStream.str();

    // Limpa memoria
    CallContextKey key = { callContext->callId, callContext->threadId };
    callContextMap.erase(key);

    delete callContext;
}


// Callbacks que serão chamados quando um evento for notificado
VOID InstGetModuleHandleA::HandleInstructionEvent(const EventData* data, void* context) {
    // Verifica o tipo de evento e processa conforme necessário
    CallContext* callContext = reinterpret_cast<CallContext*>(context);
    if (data->type == EventData::Instruction) {
        if (callContext->countInstructionsAfterCall >= 20) {
            auto instructionEvent = static_cast<const InstructionEventData*>(data);
            auto iTrace = instructionEvent->instuctionInformation.instructionTrace;

            // Copia a fila original para uma fila temporária para não alterar a fila original. std::queue não fornece acesso direto aos seus elementos além da frente e do fim da fila.
            std::queue<InstuctionInformation> tempQueue = *iTrace;

            // Desenfileira cada elemento da fila temporária e imprime
            while (!tempQueue.empty()) {
                InstuctionInformation instructionInfo = tempQueue.front();
                tempQueue.pop();
                callContext->instructionsAfterCall.push_back(instructionInfo);
            }
            
            InstGetModuleHandleA::globalNotifierPtr->Detach(callContext->instructionObserver);
            CheckPrintConditions(callContext);
        }
        else {
            callContext->countInstructionsAfterCall++;
        }
    }
}

VOID InstGetModuleHandleA::HandleTraceEvent(const EventData* data, void* context) {
    CallContext* callContext = reinterpret_cast<CallContext*>(context);
    if (data->type == EventData::CallTrace) {
        if (callContext->countFunctionsCalledAfterCall >= 20) {
            UINT32 specificThreadId = callContext->threadId;
            auto traceEvent = static_cast<const TraceEventData*>(data);
            std::stringstream& stringStream = callContext->stringStream;
            auto threadCallTracePtr = traceEvent->callTraceInfo.threadCallTraces;
            const std::map<THREADID, std::deque<CallRtnInformation>> threadCallTraces = *threadCallTracePtr;
            auto it = threadCallTraces.find(specificThreadId);
            if (it != threadCallTraces.end()) {
                const std::deque<CallRtnInformation>& callTraces = it->second;
                for (const CallRtnInformation& callInfo : callTraces) {
                    callContext->functionsCalledAfterCall.push_back(callInfo);
                }
            }
            InstGetModuleHandleA::globalNotifierPtr->Detach(callContext->traceObserver);
            CheckPrintConditions(callContext);
        }
        else {
            callContext->countFunctionsCalledAfterCall++;
        }
    }
}

VOID InstGetModuleHandleA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpModuleName) {

    PIN_LockClient();
    IMG img = IMG_FindByAddress(returnAddress);
    if (IMG_Valid(img)) {
        if (!IMG_IsMainExecutable(img)) {
            // Se a leitura não ocorre na imagem principal (executavel). Alguns protetores podem carregar funções em modulos diferentes ao principal, se for o caso, desativar o return
            return;
        }
    }

    RTN rtnCall = RTN_FindByAddress(instAddress);
    ADDRINT rtnAddress = 0;
    if (RTN_Valid(rtnCall)) {
        rtnAddress = RTN_Address(rtnCall);
    }
    PIN_UnlockClient();

    GetModuleHandleAArgs args;
    args.lpModuleName = lpModuleName;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    
    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstGetModuleHandleA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstGetModuleHandleA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstGetModuleHandleA::globalNotifierPtr->Attach(instructionObserver);
    InstGetModuleHandleA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstGetModuleHandleA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT lpModuleName) {

    PIN_LockClient();
    IMG img = IMG_FindByAddress(returnAddress);
    if (IMG_Valid(img)) {
        if (!IMG_IsMainExecutable(img)) {
            PIN_UnlockClient();
            // Se a leitura não ocorre na imagem principal (executavel). Alguns protetores podem carregar funções em modulos diferentes ao principal, se for o caso, desativar o return
            return;
        }
    }

    // Instrumentar funçao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        // Registrar parámetros
        const GetModuleHandleAArgs& args = callContext->getModuleHandleAArgs;
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpModuleName = ConvertAddrToAnsiString(args.lpModuleName);
        stringStream << std::endl << "[+] GetModuleHandleA..." << std::endl;
        stringStream << "    lpModuleName (parámetro): " << ansiStringLpModuleName << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Trace de chamadas (antes da invocação):" << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(returnAddress);
        RTN_Open(rtnCaller);

        stringStream << "    Instruções da rotina chamadora (" << RTN_Name(rtnCaller) << "):" << std::endl;
        for (INS ins = RTN_InsHead(rtnCaller); INS_Valid(ins); ins = INS_Next(ins)) {
            std::string disassembledInstruction = INS_Disassemble(ins);
            ADDRINT insAddress = INS_Address(ins);
            if (INS_IsCall(ins)) {
                // Assume que a chamada é direta para simplificar, para chamadas indiretas seria necessário mais contexto
                if (INS_IsDirectCall(ins)) {
                    ADDRINT targetAddr = INS_DirectBranchOrCallTargetAddress(ins);
                    RTN targetRtn = RTN_FindByAddress(targetAddr);
                    if (RTN_Valid(targetRtn)) {
                        std::string targetName = RTN_Name(targetRtn);
                        disassembledInstruction = disassembledInstruction + "                   ; " + targetName; // Supondo que InstuctionInfo tenha esse campo
                    }
                }
            }
            stringStream << "        " << std::hex << insAddress << std::dec << " | " << disassembledInstruction << std::endl;
        }

        RTN_Close(rtnCaller);
        PIN_UnlockClient();

    }

    fcnCallId++;
}

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn, std::ofstream& out, Notifier& globalNotifier) {

    imgCallId++;

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

    if (RTN_Name(rtn) == "GetModuleHandleA") {
        outStream = &out;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);
        RTN_Close(rtn);
    }

}

