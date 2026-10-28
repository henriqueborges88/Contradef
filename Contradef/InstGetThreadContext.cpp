#include "InstGetThreadContext.h"
#include "NtStructures.h"

CallContextMap InstGetThreadContext::callContextMap;
UINT32 InstGetThreadContext::imgCallId = 0;
CallSequenceNumber InstGetThreadContext::fcnCallId = 0;
Notifier* InstGetThreadContext::globalNotifierPtr;

/*---------------------------------------------------------*/
/* limpar DRx depois de GetThreadContext                   */
/*---------------------------------------------------------*/
//static VOID OnGetThreadContext_After(ADDRINT pCtx, BOOL retValue)
//{
//    if (!retValue) return;                 // falhou – nada a fazer
//
//    WindowsAPI::CONTEXT* ctx = reinterpret_cast<WindowsAPI::CONTEXT*>(pCtx);   // _CONTEXT da WinAPI
//    if (!PIN_CheckWriteAccess(ctx)) return;            // segurança extra
//
//    ctx->Dr0 = ctx->Dr1 = ctx->Dr2 = ctx->Dr3 = 0;
//    ctx->Dr6 = ctx->Dr7 = 0;
//    ctx->EFlags &= ~(1u << 8);       // limpa TF (bit 8)
//}

VOID InstGetThreadContext::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hThread, ADDRINT lpContext) {

    //PIN_LockClient();
    //IMG imga = IMG_FindByAddress(returnAddress);
    //std::cout << "Before Imagem " << returnAddress << std::endl;
    //if (IMG_Valid(imga))
    //    std::cout << "Before Imagem " << returnAddress << " - " << IMG_Name(imga) << std::endl;
    //PIN_UnlockClient();
    //
    //if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
    //    std::cout << "Before não é main\n";
    //    return;
    //}
    //std::cout << "Before é main\n";

    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            return;
        }
    }

    GetThreadContextArgs* args = new GetThreadContextArgs;
    args->hThread = hThread;
    args->lpContext = lpContext;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetThreadContext..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hThread: " << std::hex << hThread << std::dec << std::endl;
    stringStream << "        lpContext: " << std::hex << lpContext << std::dec << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetThreadContext" << std::endl;

}

VOID InstGetThreadContext::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal, ADDRINT lpContext) {
    //PIN_LockClient();
    //IMG imga = IMG_FindByAddress(returnAddress);
    //std::cout << "After Imagem " << returnAddress << std::endl;

    //if (IMG_Valid(imga))
    //    std::cout << "After Imagem " << returnAddress << " - " << IMG_Name(imga) << std::endl;
    //PIN_UnlockClient();

    //if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
    //    std::cout << "After não é main\n";
    //    return;
    //}
    //std::cout << "After é main\n";
    
    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
            return;
        }
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Return Value: " << retVal << std::endl;

        if (retVal != 0 && lpContext != 0) {
            // Se a chamada foi bem-sucedida, podemos exibir algumas informações do contexto
            ::WindowsAPI::CONTEXT ctxData{};
            
            PIN_SafeCopy(&ctxData, reinterpret_cast<void*>(lpContext), sizeof(::WindowsAPI::CONTEXT));

            stringStream << "    Thread Context:" << std::endl;
#ifdef _WIN64
            stringStream << "        Rax: " << std::hex << ctxData.Rax << std::dec << std::endl;
            stringStream << "        Rcx: " << std::hex << ctxData.Rcx << std::dec << std::endl;
            stringStream << "        Rdx: " << std::hex << ctxData.Rdx << std::dec << std::endl;
            stringStream << "        Rbx: " << std::hex << ctxData.Rbx << std::dec << std::endl;
            stringStream << "        Rsp: " << std::hex << ctxData.Rsp << std::dec << std::endl;
            stringStream << "        Rbp: " << std::hex << ctxData.Rbp << std::dec << std::endl;
            stringStream << "        Rsi: " << std::hex << ctxData.Rsi << std::dec << std::endl;
            stringStream << "        Rdi: " << std::hex << ctxData.Rdi << std::dec << std::endl;
            stringStream << "        Rip: " << std::hex << ctxData.Rip << std::dec << std::endl;
            stringStream << "        R8: " << std::hex << ctxData.R8 << std::dec << std::endl;
            stringStream << "        R9: " << std::hex << ctxData.R9 << std::dec << std::endl;
            stringStream << "        R10: " << std::hex << ctxData.R10 << std::dec << std::endl;
            stringStream << "        R11: " << std::hex << ctxData.R11 << std::dec << std::endl;
            stringStream << "        R12: " << std::hex << ctxData.R12 << std::dec << std::endl;
            stringStream << "        R13: " << std::hex << ctxData.R13 << std::dec << std::endl;
            stringStream << "        R14: " << std::hex << ctxData.R14 << std::dec << std::endl;
            stringStream << "        R15: " << std::hex << ctxData.R15 << std::dec << std::endl;
            stringStream << "        Xmm0: " << std::hex << ctxData.Xmm0.High << std::dec << std::endl;
            stringStream << "        Xmm1: " << std::hex << ctxData.Xmm1.High << std::dec << std::endl;
            stringStream << "        Xmm2: " << std::hex << ctxData.Xmm2.High << std::dec << std::endl;
            stringStream << "        Xmm3: " << std::hex << ctxData.Xmm3.High << std::dec << std::endl;
            stringStream << "        Xmm4: " << std::hex << ctxData.Xmm4.High << std::dec << std::endl;
            stringStream << "        Xmm5: " << std::hex << ctxData.Xmm5.High << std::dec << std::endl;
            stringStream << "        Xmm6: " << std::hex << ctxData.Xmm6.High << std::dec << std::endl;
            stringStream << "        Xmm7: " << std::hex << ctxData.Xmm7.High << std::dec << std::endl;
            stringStream << "        Xmm8: " << std::hex << ctxData.Xmm8.High << std::dec << std::endl;
            stringStream << "        Xmm9: " << std::hex << ctxData.Xmm9.High << std::dec << std::endl;
            stringStream << "        Xmm10: " << std::hex << ctxData.Xmm10.High << std::dec << std::endl;
            stringStream << "        Xmm11: " << std::hex << ctxData.Xmm11.High << std::dec << std::endl;
            stringStream << "        Xmm12: " << std::hex << ctxData.Xmm12.High << std::dec << std::endl;
            stringStream << "        Xmm13: " << std::hex << ctxData.Xmm13.High << std::dec << std::endl;
            stringStream << "        Xmm14: " << std::hex << ctxData.Xmm14.High << std::dec << std::endl;
            stringStream << "        Xmm15: " << std::hex << ctxData.Xmm15.High << std::dec << std::endl;
            stringStream << "        SegCs: " << std::hex << ctxData.SegCs << std::dec << std::endl;
            stringStream << "        SegDs: " << std::hex << ctxData.SegDs << std::dec << std::endl;
            stringStream << "        SegEs: " << std::hex << ctxData.SegEs << std::dec << std::endl;
            stringStream << "        SegFs: " << std::hex << ctxData.SegFs << std::dec << std::endl;
            stringStream << "        SegGs: " << std::hex << ctxData.SegGs << std::dec << std::endl;
            stringStream << "        SegSs: " << std::hex << ctxData.SegSs << std::dec << std::endl;
            stringStream << "        EFlags: " << std::hex << ctxData.EFlags << std::dec << std::endl;
            stringStream << "        Dr0: " << std::hex << ctxData.Dr0 << std::dec << std::endl;
            stringStream << "        Dr1: " << std::hex << ctxData.Dr1 << std::dec << std::endl;
            stringStream << "        Dr2: " << std::hex << ctxData.Dr2 << std::dec << std::endl;
            stringStream << "        Dr3: " << std::hex << ctxData.Dr3 << std::dec << std::endl;
            stringStream << "        Dr6: " << std::hex << ctxData.Dr6 << std::dec << std::endl;
            stringStream << "        Dr7: " << std::hex << ctxData.Dr7 << std::dec << std::endl;
#else
            stringStream << "        Eax: " << std::hex << ctxData.Eax << std::dec << std::endl;
            stringStream << "        Ecx: " << std::hex << ctxData.Ecx << std::dec << std::endl;
            stringStream << "        Edx: " << std::hex << ctxData.Edx << std::dec << std::endl;
            stringStream << "        Ebx: " << std::hex << ctxData.Ebx << std::dec << std::endl;
            stringStream << "        Esp: " << std::hex << ctxData.Esp << std::dec << std::endl;
            stringStream << "        Ebp: " << std::hex << ctxData.Ebp << std::dec << std::endl;
            stringStream << "        Esi: " << std::hex << ctxData.Esi << std::dec << std::endl;
            stringStream << "        Edi: " << std::hex << ctxData.Edi << std::dec << std::endl;
            stringStream << "        Eip: " << std::hex << ctxData.Eip << std::dec << std::endl;
            stringStream << "        EFlags: " << std::hex << ctxData.EFlags << std::dec << std::endl;
            stringStream << "        SegCs: " << std::hex << ctxData.SegCs << std::dec << std::endl;
            stringStream << "        SegDs: " << std::hex << ctxData.SegDs << std::dec << std::endl;
            stringStream << "        SegEs: " << std::hex << ctxData.SegEs << std::dec << std::endl;
            stringStream << "        SegFs: " << std::hex << ctxData.SegFs << std::dec << std::endl;
            stringStream << "        SegGs: " << std::hex << ctxData.SegGs << std::dec << std::endl;
            stringStream << "        SegSs: " << std::hex << ctxData.SegSs << std::dec << std::endl;
            stringStream << "        Dr0: " << std::hex << ctxData.Dr0 << std::dec << std::endl;
            stringStream << "        Dr1: " << std::hex << ctxData.Dr1 << std::dec << std::endl;
            stringStream << "        Dr2: " << std::hex << ctxData.Dr2 << std::dec << std::endl;
            stringStream << "        Dr3: " << std::hex << ctxData.Dr3 << std::dec << std::endl;
            stringStream << "        Dr6: " << std::hex << ctxData.Dr6 << std::dec << std::endl;
            stringStream << "        Dr7: " << std::hex << ctxData.Dr7 << std::dec << std::endl;
#endif
        }

        stringStream << "  [-] Call from GetThreadContext completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetThreadContextArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetThreadContext::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetThreadContext") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x4;
            ADDRINT base = RTN_Address(rtn);
            for (INS i = RTN_InsHead(rtn);
                INS_Valid(i) && INS_Address(i) <= base + scanSize;
                i = INS_Next(i))
            {

                if (INS_Address(i) == base + scanSize)
                {
                    INS_InsertCall(i, IPOINT_BEFORE,
                        (AFUNPTR)CallbackBefore,
                        IARG_THREAD_ID,
                        IARG_UINT32, imgCallId,
                        IARG_INST_PTR,
                        IARG_ADDRINT, RTN_Address(rtn),
                        IARG_CONTEXT,
                        IARG_RETURN_IP,                        // Endereço da função chamante
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // hThread
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // lpContext
                        IARG_END);
                }
            }
        }
        else {
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
        }

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // lpContext
            IARG_END);

        //RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(OnGetThreadContext_After),
        //    IARG_FUNCRET_EXITPOINT_VALUE,      // BOOL retorno
        //    IARG_FUNCARG_ENTRYPOINT_VALUE, 1,   // ptr CONTEXT
        //    IARG_END);

        RTN_Close(rtn);
    }
}
