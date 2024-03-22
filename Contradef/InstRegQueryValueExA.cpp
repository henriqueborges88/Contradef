#include "InstRegQueryValueExA.h"

std::map<CallContextKey, CallContext*> InstRegQueryValueExA::callContextMap;
UINT32 InstRegQueryValueExA::imgCallId = 0;
UINT32 InstRegQueryValueExA::fcnCallId = 0;
Notifier* InstRegQueryValueExA::globalNotifierPtr;

VOID InstRegQueryValueExA::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstRegQueryValueExA::callContextMap, InstRegQueryValueExA::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstRegQueryValueExA::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstRegQueryValueExA::callContextMap, InstRegQueryValueExA::globalNotifierPtr);
}

VOID InstRegQueryValueExA::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstRegQueryValueExA::callContextMap, InstRegQueryValueExA::globalNotifierPtr);
}

VOID InstRegQueryValueExA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstRegQueryValueExAArgs args;
    args.hKey = hKey;
    args.lpValueName = lpValueName;
    args.lpReserved = lpReserved;
    args.lpType = lpType;
    args.lpData = lpData;
    args.lpcbData = lpcbData;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstRegQueryValueExA::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstRegQueryValueExA::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstRegQueryValueExA::globalNotifierPtr->Attach(instructionObserver);
    InstRegQueryValueExA::globalNotifierPtr->Attach(traceObserver);
}

VOID InstRegQueryValueExA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
   
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
        const InstRegQueryValueExAArgs* args = reinterpret_cast<InstRegQueryValueExAArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;
        std::string ansiStringlpValueName = ConvertAddrToAnsiString(args->lpValueName);
        stringStream << std::endl << "[+] RegQueryValueExA..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Id de chamada: " << fcnCallId << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parámetros: " << std::endl;
        stringStream << "        hKey: " << args->hKey << std::endl;
        stringStream << "        lpValueName: " << ansiStringlpValueName << std::endl;
        stringStream << "        lpReserved: " << args->lpReserved << std::endl;
        stringStream << "        lpType: " << args->lpType << std::endl;
        stringStream << "        lpData: " << args->lpData << std::endl;
        stringStream << "        lpcbData: " << args->lpcbData << std::endl;
        stringStream << "    Valor de retorno: " << *retValAddr << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        RTN rtnCaller = RTN_FindByAddress(returnAddress);
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

VOID InstRegQueryValueExA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
            IARG_END);

        RTN_Close(rtn);
    }
}