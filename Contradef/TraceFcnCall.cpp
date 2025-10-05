#include "TraceFcnCall.h"

namespace TraceFcnCall {
    // Arquivo de saída para rastreamento de chamadas externas
    static std::ofstream ExternalCallTraceOutFile;
    static std::ofstream ExternalCallTraceOutFileM2;
    
    PIN_MUTEX traceFcnMutex;
    std::map<THREADID, CallStackManager> callStacks;
    DllFunctionTracker callTracker(100000); // Máximo de 100000 funções rastreadas
    bool saveAtFini = false;

    VOID LogFuncCall(THREADID threadid, ADDRINT instAddress, ADDRINT targetRtnAddr) {

        std::string tgtRtnName = "";
        std::string imgName = "";
        ADDRINT tgtRtnAddr = 0;
        PIN_LockClient();
        RTN tgtRtn = RTN_FindByAddress(targetRtnAddr);
        PIN_UnlockClient();
        if (RTN_Valid(tgtRtn))
        {
            tgtRtnName = RTN_Name(tgtRtn);
            tgtRtnAddr = RTN_Address(tgtRtn);
            IMG tgtImg = SEC_Img(RTN_Sec(tgtRtn));
            if (IMG_Valid(tgtImg))
            {
                imgName = IMG_Name(tgtImg);
            }
        }
        if (tgtRtnName.empty()) {
            OS_PROCESS_ID pid = PIN_GetPid();
            FunctionData fcnData = GetFunctionFromAddress(targetRtnAddr, pid);
            tgtRtnName = fcnData.functionName;
            tgtRtnAddr = targetRtnAddr;
            imgName = "[forced] " + fcnData.fullModuleName;
        }
        PIN_MutexLock(&traceFcnMutex);
        callTracker.addFunctionCall(threadid, imgName, tgtRtnName, tgtRtnAddr);
        if (KnobTraceExternalCallTrace && !saveAtFini) {
            saveLastExternalCall(ExternalCallTraceOutFile);
        }
        PIN_MutexUnlock(&traceFcnMutex);

    }

    VOID EmitFuncCall(THREADID threadid, ADDRINT instAddress, ADDRINT targetRtnAddr, BOOL isCall, BOOL isDirect, INT32 tailCall, ADDRINT returnAddress, ADDRINT rspValue, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
    {
        if (targetRtnAddr == 0) {
            return;
        }

        if (IsMainExecutable(instAddress) && !IsMainExecutable(targetRtnAddr)) {
            LogFuncCall(threadid, instAddress, targetRtnAddr);
        }
    }


    VOID EmitFuncReturn(THREADID threadid, ADDRINT instAddress, ADDRINT retAddr, ADDRINT retVal, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
    {
        //retAddr -> target

        ///// Desativando call stack. Ainda em testes
        //if (IsMainExecutable(retAddr)) {
        //    callStacks[threadid].popUntilAddress(retAddr);
        //    
        //    //callStacks[threadid].popFunction();
        //    //callStackManager.peekTopFunction();
        //}

        if (retAddr == 0) {
            return;
        }

        if (IsMainExecutable(instAddress) /* origem precisa ser executável principal */ && !IsMainExecutable(retAddr) /* destino fora do executável principal? */) {
            LogFuncCall(threadid, instAddress, retAddr);
        }
    }

    VOID InstrumentFuncCall(TRACE trace, INS ins)
    {
        if ((INS_IsCall(ins) && !INS_IsDirectControlFlow(ins)) || INS_Category(ins) == XED_CATEGORY_UNCOND_BR)
        {
            BOOL isCall = INS_IsCall(ins);
            // Indirect call
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
                IARG_THREAD_ID,
                IARG_INST_PTR,
                IARG_BRANCH_TARGET_ADDR,
                IARG_BOOL, isCall, // isCall
                IARG_BOOL, FALSE, // isDirect
                IARG_BOOL, FALSE, // tailCall
                IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
                IARG_REG_VALUE, REG_STACK_PTR,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_FUNCARG_CALLSITE_VALUE, 6,
                IARG_END);
        }
        else if (!INS_IsRet(ins) && INS_IsIndirectControlFlow(ins))
        {
            BOOL isCall = INS_IsCall(ins);

            // Indirect call
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
                IARG_THREAD_ID,
                IARG_INST_PTR,
                IARG_BRANCH_TARGET_ADDR,
                IARG_BOOL, isCall, // isCall
                IARG_BOOL, FALSE, // isDirect
                IARG_BOOL, FALSE, // tailCall
                IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
                IARG_REG_VALUE, REG_STACK_PTR,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_FUNCARG_CALLSITE_VALUE, 6,
                IARG_END);
        }
        else if (INS_IsDirectControlFlow(ins))
        {
            // Is this a tail call?
            PIN_LockClient();
            RTN sourceRtn = TRACE_Rtn(trace);
            RTN destRtn = RTN_FindByAddress(INS_DirectControlFlowTargetAddress(ins));
            PIN_UnlockClient();

            //std::cout << INS_Disassemble(ins) << '\n';

            if (INS_IsCall(ins)         // conventional call
                || sourceRtn != destRtn // tail call
                )
            {
                BOOL isCall = INS_IsCall(ins);
                BOOL tailcall = !isCall;
                INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
                    IARG_THREAD_ID,
                    IARG_INST_PTR,
                    IARG_ADDRINT, RTN_Address(destRtn),
                    IARG_BOOL, isCall, // isCall
                    IARG_BOOL, TRUE, // isDirect
                    IARG_BOOL, tailcall,
                    IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
                    IARG_REG_VALUE, REG_STACK_PTR,
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
        else if (INS_IsRet(ins))
        {
            RTN rtn = TRACE_Rtn(trace);

#if defined(TARGET_LINUX) && defined(TARGET_IA32)
            //        if( RTN_Name(rtn) ==  "_dl_debug_state") return;
            if (RTN_Valid(rtn) && RTN_Name(rtn) == "_dl_runtime_resolve") return;
#endif
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncReturn),
                IARG_THREAD_ID,
                IARG_INST_PTR,                 // origem
                IARG_BRANCH_TARGET_ADDR,       // destino
                IARG_FUNCRET_EXITPOINT_VALUE,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_FUNCARG_CALLSITE_VALUE, 6,
                IARG_END);

        }
        else {
            if (INS_IsDirectControlFlow(ins)) {

                std::cout << INS_Disassemble(ins) << "\n";
            }
        }
    }

    VOID TraceFunc(TRACE trace, VOID* v)
    {
        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        {
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            {
                InstrumentFuncCall(trace, ins);
            }
        }
    }

    VOID saveAllExternalCalls(std::ofstream& ExternalCallTraceOutFile)
    {
        callTracker.saveAllExternalCalls(ExternalCallTraceOutFile);
    }

    VOID saveLastExternalCall(std::ofstream& ExternalCallTraceOutFile)
    {
        callTracker.saveLastExternalCall(ExternalCallTraceOutFile);
    }

    VOID Fini(int, VOID* v) {
        if (KnobTraceExternalCallTrace && saveAtFini) {
            saveAllExternalCalls(ExternalCallTraceOutFile);
        }
        
        ExternalCallTraceOutFile.close();
        ExternalCallTraceOutFileM2.close();
    }

    int Init(std::string pid, std::string filename)
    {

        if (KnobTraceExternalCallTrace)
        {
            std::string logfilename = filename + "." + pid + ".TraceFcnCall.M1.cdf";
            ExternalCallTraceOutFile.open(logfilename.c_str(), std::ios::binary);
        }

        PIN_AddFiniFunction(Fini, 0);
        TRACE_AddInstrumentFunction(TraceFunc, 0);

        return 0;
    }
}