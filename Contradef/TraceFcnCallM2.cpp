#include "TraceFcnCallM2.h"
#include "InstrumentationUtils.h"
#include <iomanip>
#include "InitParams.h"
#include "utils.h"
#include "TraceInstructions.h"

namespace TraceFcnCallM2
{
    //gExternalCallsOnly = FALSE -> Todas as chamadas saindo do executable principal(para dentro ou fora).
    static BOOL gExternalCallsOnly = TRUE;

    static std::ofstream   gOut;
    static PIN_MUTEX       gLock;
    static INT32           gEnabled = 0;

    static std::stack<ADDRINT> gCallStack;
    static INT32          gIndent = 0;          // para formatação “árvore”
    BOOL enableIdent = true;
    /* ----- helpers -------------------------------------------- */
    static inline bool IsOn(void) { return gEnabled != 0; }

    static VOID Indent()
    {
        for (INT32 i = 0; i < gIndent; ++i) gOut << "| ";
    }

    static inline bool WantCall(ADDRINT src, ADDRINT dst)
    {
        //if (!gExternalCallsOnly)
        //{   // modo normal: obedece apenas ao gMainExeOnly
        //    return (!gMainExeOnly) || IsMainExecutable(src);
        //}

        /* gExternalCallsOnly == TRUE :
           só grava se sair do main e entrar num módulo externo      */
        if (IsMainExecutable(src) && !IsMainExecutable(dst)) {
            return TRUE;
        }

        if ((!gExternalCallsOnly && IsMainExecutable(src) && IsMainExecutable(dst))) {
            PIN_LockClient();
            RTN rtnSrc = RTN_FindByAddress(src);
            RTN rtnDst = RTN_FindByAddress(dst);
            PIN_UnlockClient();

            if (RTN_Valid(rtnSrc) && RTN_Valid(rtnDst)) {
                if (RTN_Id(rtnSrc) != RTN_Id(rtnDst)) {
                    return TRUE;
                }
            }

        }
        return FALSE;
    }

    static inline bool WantRet(ADDRINT retIp /*no callee*/,
        ADDRINT nextIp /*vai voltar p/ aqui*/)
    {
        //if (!gExternalCallsOnly)
        //    return (!gMainExeOnly) || IsMainExecutable(retIp);

        /* precisamos do par DLL → main                    */
        return (!IsMainExecutable(retIp) && IsMainExecutable(nextIp)) || ((!gExternalCallsOnly && IsMainExecutable(retIp) && IsMainExecutable(nextIp)));
    }

    std::string getStringFromAddr(ADDRINT address) {

        std::string suffix;
        std::string extracted = FormatPtrOrValue(address, 128);
        VOID* ea = reinterpret_cast<VOID*>(address);
        if (!extracted.empty())
        {
            suffix = " -> \"" + extracted + "\"";
        }
        else if (!PIN_CheckReadAccess(ea))
        {   // sem acesso: mostra bytes como ASCII “forçado”
            std::string strFromVal = InterpretAddrIntAsASCII(address);
            if (strFromVal.empty())
                return "";
            suffix = " -> \"" + strFromVal + "\"";
        }
        return suffix;
    }

    VOID LogFuncCall(THREADID tid,
        ADDRINT ip,      /* src  */
        ADDRINT target,  /* dst  */
        BOOL   isTail,
        ADDRINT arg0, ADDRINT arg1, ADDRINT arg2,
        ADDRINT arg3, ADDRINT arg4, ADDRINT arg5) {

        PIN_MutexLock(&gLock);
        Indent();

        PIN_LockClient();
        RTN rtnIp = RTN_FindByAddress(ip);
        RTN rtnTarget = RTN_FindByAddress(target);
        PIN_UnlockClient();

        gOut << (isTail ? "Tailcall from " : "Call from ")
            << FormatAddress(ip, rtnIp,
                KnobSymbols, KnobFullImgName, KnobLines)
            << " -> "
            << FormatAddress(target, rtnTarget,
                KnobSymbols, KnobFullImgName, KnobLines)
            << "  [T" << tid << "] ("
            << std::hex << arg0 << getStringFromAddr(arg0) << ", " << arg1 << getStringFromAddr(arg1) << ", " << arg2 << getStringFromAddr(arg2) << ", "
            << arg3 << getStringFromAddr(arg3) << ", " << arg4 << getStringFromAddr(arg4) << ", " << arg5 << getStringFromAddr(arg5) << ", ...)\n";
        if (enableIdent) ++gIndent;
        PIN_MutexUnlock(&gLock);

    }

    // --- call directa ------------------------------------------------
    static VOID EmitDirectCall(THREADID tid,
        ADDRINT ip,      /* src  */
        ADDRINT target,  /* dst  */
        BOOL   isTail,
        ADDRINT arg0, ADDRINT arg1, ADDRINT arg2,
        ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
    {
        if (!IsOn() || !WantCall(ip, target)) return;

        if (isTail)
        {
            if (enableIdent && gIndent) --gIndent;
        }
        else
        {
            gCallStack.push(ip);
        }
        gCallStack.push(ip);

        LogFuncCall(tid, ip, target, isTail, arg0, arg1, arg2, arg3, arg4, arg5);
    }

    // --- call indirecta ----------------------------------------------
    static VOID EmitIndirectCall(THREADID tid, ADDRINT ip, ADDRINT target,
        ADDRINT arg0, ADDRINT arg1, ADDRINT arg2,
        ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
    {
        if (!IsOn() || !WantCall(ip, target)) return;

        gCallStack.push(ip);

        LogFuncCall(tid, ip, target, false, arg0, arg1, arg2, arg3, arg4, arg5);
    }

    // --- retorno -----------------------------------------------------
    static VOID EmitReturn(THREADID tid,
        ADDRINT retIp,   // endereço da instrução RET
        ADDRINT nextIp,  // aonde irá saltar
        ADDRINT* retVal,
        ADDRINT arg0, ADDRINT arg1, ADDRINT arg2,
        ADDRINT arg3, ADDRINT arg4, ADDRINT arg5)
    {
        if (!IsOn()) return;

        // Fallback para o Themida - TODO: Usar o condicional do YARA para o Themida
        if (IsStringInStrategyScope("Themida")) {
            if (IsMainExecutable(retIp) && !IsMainExecutable(nextIp)) {
                LogFuncCall(tid, retIp, nextIp, false, arg0, arg1, arg2, arg3, arg4, arg5);
                return;
            }
        }
        // Fin do Fallback para o Themida 
        
        if (!IsMainExecutable(retIp) && IsMainExecutable(nextIp)) {
            //std::cout << "Addr: " << nextIp << '\n';
        }

        if (!WantRet(retIp, nextIp)) return;

        //if (gCallStack.empty())
        //    return;                     // RET interno nas dlls chamadas – descarta-se para considerer ret apenas para o main

        /*  estamos a gravar só para chamadas registadas,
            logo a pilha está consistente                       */
        if (enableIdent && gIndent) --gIndent;

        ADDRINT callIp = 0;
        if (!gCallStack.empty()) {
            callIp = gCallStack.top();
            gCallStack.pop();
        }

        PIN_MutexLock(&gLock);
        Indent();
        PIN_LockClient();
        RTN rtnRetIp = RTN_FindByAddress(retIp);
        PIN_UnlockClient();
        
        gOut << "Ret from   "
            << FormatAddress(retIp, rtnRetIp,
                KnobSymbols, KnobFullImgName, KnobLines)
            << "  [T" << std::dec << tid << "] Return value -> " << std::hex << *retVal << getStringFromAddr(retValue)
            << "  (next " << nextIp << ", came-from " << callIp << ")\n";
        PIN_MutexUnlock(&gLock);
    }

    static VOID InstrumentIns(TRACE trace, INS ins)
    {
        /* ---------- 1. CALL indirecto ---------------------------------- */
        if (INS_IsCall(ins) && !INS_IsDirectControlFlow(ins))
        {
            INS_InsertCall(ins, IPOINT_BEFORE,
                (AFUNPTR)EmitIndirectCall,
                IARG_THREAD_ID,
                IARG_INST_PTR,
                IARG_BRANCH_TARGET_ADDR,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_END);
            return;
        }

        /* ---------- 2. Fluxo directo (CALL ou JMP) ---------------------- */
        if (INS_IsDirectControlFlow(ins))
        {
            ADDRINT dst = INS_DirectControlFlowTargetAddress(ins);

            /* -- filtro “auto-chamada”: ignora se é o mesmo RTN ---------- */
            RTN srcRtn = TRACE_Rtn(trace);
            PIN_LockClient();
            RTN dstRtn = RTN_FindByAddress(dst);
            PIN_UnlockClient();

            if (RTN_Valid(srcRtn) && RTN_Valid(dstRtn) && srcRtn == dstRtn)
                return;                     // chamada recursiva na mesma rotina → não logar

            /* -------- 2a. CALL directo “normal” ------------------------ */
            if (INS_IsCall(ins))
            {
                INS_InsertCall(ins, IPOINT_BEFORE,
                    (AFUNPTR)EmitDirectCall,
                    IARG_THREAD_ID,
                    IARG_INST_PTR,            /* src  */
                    IARG_ADDRINT, dst,        /* dst  */
                    IARG_BOOL, FALSE,         /* tail? */
                    IARG_FUNCARG_CALLSITE_VALUE, 0,
                    IARG_FUNCARG_CALLSITE_VALUE, 1,
                    IARG_FUNCARG_CALLSITE_VALUE, 2,
                    IARG_FUNCARG_CALLSITE_VALUE, 3,
                    IARG_FUNCARG_CALLSITE_VALUE, 4,
                    IARG_FUNCARG_CALLSITE_VALUE, 5,
                    IARG_END);
                return;
            }

            /* -------- 2b. POSSÍVEL tail-call (JMP directo) -------------- */
            if (INS_IsBranch(ins) &&      // é algum branch
                !INS_IsCall(ins) &&      // mas não é CALL
                !INS_IsIndirectBranchOrCall(ins) && // é directo
                !INS_HasFallThrough(ins) &&      // não há fall-through => último da BBL
                RTN_Valid(srcRtn) && RTN_Valid(dstRtn) && srcRtn != dstRtn)
            {

                INS_InsertCall(ins, IPOINT_BEFORE,
                    (AFUNPTR)EmitDirectCall,
                    IARG_THREAD_ID,
                    IARG_INST_PTR,            /* src  */
                    IARG_ADDRINT, dst,        /* dst  */
                    IARG_BOOL, TRUE,          /* tail? */
                    IARG_FUNCARG_CALLSITE_VALUE, 0,
                    IARG_FUNCARG_CALLSITE_VALUE, 1,
                    IARG_FUNCARG_CALLSITE_VALUE, 2,
                    IARG_FUNCARG_CALLSITE_VALUE, 3,
                    IARG_FUNCARG_CALLSITE_VALUE, 4,
                    IARG_FUNCARG_CALLSITE_VALUE, 5,
                    IARG_END);
            }
            return;
        }

        /* ---------- 3. Tail-call por JMP indirecto ---------------------- */
        if (INS_IsBranch(ins) &&  // é um branch…
            !INS_IsCall(ins) &&  // …que não é CALL
            !INS_IsDirectControlFlow(ins))   // …e o alvo é indirecto
        {
            INS_InsertCall(ins, IPOINT_BEFORE,
                (AFUNPTR)EmitDirectCall,
                IARG_THREAD_ID,
                IARG_INST_PTR,               /* src  */
                IARG_BRANCH_TARGET_ADDR,     /* dst  (run-time) */
                IARG_BOOL, TRUE,             /* tail? */
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_END);
            return;
        }

        /* ---------- 4. RET --------------------------------------------- */
        if (INS_IsRet(ins))
        {
            INS_InsertCall(ins, IPOINT_BEFORE,
                (AFUNPTR)EmitReturn,
                IARG_THREAD_ID,
                IARG_INST_PTR,                 // origem
                IARG_BRANCH_TARGET_ADDR,       // destino
                IARG_FUNCRET_EXITPOINT_REFERENCE,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_FUNCARG_CALLSITE_VALUE, 6,
                IARG_END);
        }
    }


    VOID Trace(TRACE trace, VOID*)
    {
        if (!IsOn()) return;

        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        {
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
                InstrumentIns(trace, ins);
        }
    }

    INT32 Init(std::string pid, std::string filename)
    {
        filename += "." + pid + ".TraceFcnCall.M2.cdf";
        gOut.open(filename.c_str());
        gOut << std::hex << std::right << std::showbase;

        PIN_MutexInit(&gLock);

        TRACE_AddInstrumentFunction(Trace, nullptr);
        if (true || IsStringInStrategyScope("Themida")) {
            enableIdent = false; // Desativando identação devido ao formato de chamadas do Themida
        }
        gEnabled = 1;
        return 0;
    }

    void Enable() { gEnabled = 1; }
    void Disable() { gEnabled = 0; }

} // namespace TraceFcnCallM2
