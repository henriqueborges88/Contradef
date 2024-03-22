#include "InstRegOpenKeyA.h"

std::map<CallContextKey, CallContext*> InstRegOpenKeyA::callContextMap;
UINT32 InstRegOpenKeyA::imgCallId = 0;
UINT32 InstRegOpenKeyA::fcnCallId = 0;
Notifier* InstRegOpenKeyA::globalNotifierPtr;

VOID InstRegOpenKeyA::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstRegOpenKeyA::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

VOID InstRegOpenKeyA::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegOpenKeyA::callContextMap, InstRegOpenKeyA::globalNotifierPtr);
}

VOID InstRegOpenKeyA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegOpenKeyAArgs args;
    args.hKey = hKey;
    args.lpSubKey = lpSubKey;
    args.phkResult = phkResult;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegOpenKeyA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegOpenKeyA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegOpenKeyA::globalNotifierPtr->Attach(instructionObserver);
    InstRegOpenKeyA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegOpenKeyA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult) {
   
    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar funçao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        // Registrar parámetros
        const InstRegOpenKeyAArgs* args = reinterpret_cast<InstRegOpenKeyAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringLpSubKey = ConvertAddrToAnsiString(args->lpSubKey);
        if (ansiStringLpSubKey == "HARDWARE\ACPI\DSDT\VBOX__") {
            // Contramedida pode ser alterando o valor de retorno para algumas chaves
            //*retValAddr = 0;
        }
        stringStream << std::endl << "[+] RegOpenKeyA..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpSubKey: " << ansiStringLpSubKey << std::endl;
        stringStream << "        phkResult: " << args->phkResult << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(*retValAddr);
        if (RTN_Valid(rtnCaller)) {
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
                            disassembledInstruction = disassembledInstruction + "                           ; " + targetName; // Supondo que InstuctionInfo tenha esse campo
                        }
                    }
                }
                stringStream << "        " << std::hex << insAddress << std::dec << " | " << disassembledInstruction << std::endl;
            }

            RTN_Close(rtnCaller);
        }
        callContext->defaultPrintReady = true;
        PIN_UnlockClient();
    }

    fcnCallId++;

}

VOID InstRegOpenKeyA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "RegOpenKeyA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // phkResult
            IARG_END);

        RTN_Close(rtn);
    }
}

