#include "InstCreateProcess.h"

CallContextMap InstCreateProcess::callContextMap;
UINT32 InstCreateProcess::imgCallId = 0;
CallSequenceNumber InstCreateProcess::fcnCallId = 0;
Notifier* InstCreateProcess::globalNotifierPtr;


VOID InstCreateProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpApplicationName, ADDRINT lpCommandLine, ADDRINT lpProcessAttributes, ADDRINT lpThreadAttributes,
    ADDRINT bInheritHandles, ADDRINT dwCreationFlags, ADDRINT lpEnvironment, ADDRINT lpCurrentDirectory,
    ADDRINT lpStartupInfo, ADDRINT lpProcessInformation) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstCreateProcessArgs* args = new InstCreateProcessArgs;
    args->lpApplicationName = lpApplicationName;
    args->lpCommandLine = lpCommandLine;
    args->lpProcessAttributes = lpProcessAttributes;
    args->lpThreadAttributes = lpThreadAttributes;
    args->bInheritHandles = bInheritHandles;
    args->dwCreationFlags = dwCreationFlags;
    args->lpEnvironment = lpEnvironment;
    args->lpCurrentDirectory = lpCurrentDirectory;
    args->lpStartupInfo = lpStartupInfo;
    args->lpProcessInformation = lpProcessInformation;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    }

VOID InstCreateProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT lpApplicationName, ADDRINT lpCommandLine, ADDRINT lpProcessAttributes, ADDRINT lpThreadAttributes,
    ADDRINT bInheritHandles, ADDRINT dwCreationFlags, ADDRINT lpEnvironment, ADDRINT lpCurrentDirectory,
    ADDRINT lpStartupInfo, ADDRINT lpProcessInformation) {

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
        const InstCreateProcessArgs* args = reinterpret_cast<InstCreateProcessArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        std::string applicationName = args->lpApplicationName ? ConvertAddrToAnsiString(args->lpApplicationName) : "NULL";
        std::string commandLine = args->lpCommandLine ? ConvertAddrToAnsiString(args->lpCommandLine) : "NULL";

        
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        lpApplicationName: " << applicationName << std::endl;
        stringStream << "        lpCommandLine: " << commandLine << std::endl;
        stringStream << "        lpProcessAttributes: " << args->lpProcessAttributes << std::endl;
        stringStream << "        lpThreadAttributes: " << args->lpThreadAttributes << std::endl;
        stringStream << "        bInheritHandles: " << args->bInheritHandles << std::endl;
        stringStream << "        dwCreationFlags: " << args->dwCreationFlags << std::endl;
        stringStream << "        lpEnvironment: " << args->lpEnvironment << std::endl;
        stringStream << "        lpCurrentDirectory: " << args->lpCurrentDirectory << std::endl;
        stringStream << "        lpStartupInfo: " << args->lpStartupInfo << std::endl;
        stringStream << "        lpProcessInformation: " << args->lpProcessInformation << std::endl;
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

VOID InstCreateProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateProcessA" || rtnName == "CreateProcessW") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpApplicationName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpCommandLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpProcessAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpThreadAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // bInheritHandles
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwCreationFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpEnvironment
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpCurrentDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpStartupInfo
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpProcessInformation
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpApplicationName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpCommandLine
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpProcessAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpThreadAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // bInheritHandles
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // dwCreationFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpEnvironment
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpCurrentDirectory
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lpStartupInfo
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // lpProcessInformation
            IARG_END);

        RTN_Close(rtn);
    }
}

