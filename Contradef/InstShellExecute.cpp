#include "InstShellExecute.h"

CallContextMap InstShellExecute::callContextMap;
UINT32 InstShellExecute::imgCallId = 0;
CallSequenceNumber InstShellExecute::fcnCallId = 0;
Notifier* InstShellExecute::globalNotifierPtr;


VOID InstShellExecute::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hwnd, ADDRINT lpOperation, ADDRINT lpFile, ADDRINT lpParameters, ADDRINT lpDirectory, ADDRINT nShowCmd) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstShellExecuteArgs* args = new InstShellExecuteArgs;
    args->hwnd = hwnd;
    args->lpOperation = lpOperation;
    args->lpFile = lpFile;
    args->lpParameters = lpParameters;
    args->lpDirectory = lpDirectory;
    args->nShowCmd = nShowCmd;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstShellExecute::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT hwnd, ADDRINT lpOperation, ADDRINT lpFile, ADDRINT lpParameters, ADDRINT lpDirectory, ADDRINT nShowCmd) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = pendingCallContext;
        // Registrar Parámetros
        const InstShellExecuteArgs* args = reinterpret_cast<InstShellExecuteArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string operation = args->lpOperation ? ConvertAddrToAnsiString(args->lpOperation) : "NULL";
        std::string file = args->lpFile ? ConvertAddrToAnsiString(args->lpFile) : "NULL";
        std::string parameters = args->lpParameters ? ConvertAddrToAnsiString(args->lpParameters) : "NULL";
        std::string directory = args->lpDirectory ? ConvertAddrToAnsiString(args->lpDirectory) : "NULL";

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hwnd: " << args->hwnd << std::endl;
        stringStream << "        lpOperation: " << operation << std::endl;
        stringStream << "        lpFile: " << file << std::endl;
        stringStream << "        lpParameters: " << parameters << std::endl;
        stringStream << "        lpDirectory: " << directory << std::endl;
        stringStream << "        nShowCmd: " << args->nShowCmd << std::endl;
        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstShellExecute::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (RTN_Name(rtn) == "ShellExecuteA" || RTN_Name(rtn) == "ShellExecuteW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpOperation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpParameters
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nShowCmd
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hwnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpOperation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpParameters
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // nShowCmd
            IARG_END);

        RTN_Close(rtn);
    }
}
