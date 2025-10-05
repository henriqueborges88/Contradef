#include "InstNtCreateThreadEx.h"

std::map<CallContextKey, CallContext*> InstNtCreateThreadEx::callContextMap;
UINT32 InstNtCreateThreadEx::imgCallId = 0;
UINT32 InstNtCreateThreadEx::fcnCallId = 0;
Notifier* InstNtCreateThreadEx::globalNotifierPtr;


VOID InstNtCreateThreadEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT ThreadHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ProcessHandle,
    ADDRINT StartAddress, ADDRINT Parameter, ADDRINT CreateFlags, ADDRINT ZeroBits,
    ADDRINT StackSize, ADDRINT MaximumStackSize, ADDRINT AttributeList) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstNtCreateThreadExArgs* args = new InstNtCreateThreadExArgs;
    args->ThreadHandle = ThreadHandle;
    args->DesiredAccess = DesiredAccess;
    args->ObjectAttributes = ObjectAttributes;
    args->ProcessHandle = ProcessHandle;
    args->StartAddress = StartAddress;
    args->Parameter = Parameter;
    args->CreateFlags = CreateFlags;
    args->ZeroBits = ZeroBits;
    args->StackSize = StackSize;
    args->MaximumStackSize = MaximumStackSize;
    args->AttributeList = AttributeList;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    std::stringstream& stringStream = callContext->stringStream;

    PIN_LockClient();
    IMG img = IMG_FindByAddress(instAddress);
    RTN rtnCurrent = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
    stringStream << "    Module name: " << IMG_Name(img) << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        ThreadHandle: " << ThreadHandle << std::endl;
    stringStream << "        DesiredAccess: " << DesiredAccess << std::endl;
    stringStream << "        ObjectAttributes: " << ObjectAttributes << std::endl;
    stringStream << "        ProcessHandle: " << ProcessHandle << std::endl;
    stringStream << "        StartAddress: " << StartAddress << std::endl;
    stringStream << "        Parameter: " << Parameter << std::endl;
    stringStream << "        CreateFlags: " << CreateFlags << std::endl;
    stringStream << "        ZeroBits: " << ZeroBits << std::endl;
    stringStream << "        StackSize: " << StackSize << std::endl;
    stringStream << "        MaximumStackSize: " << MaximumStackSize << std::endl;
    stringStream << "        AttributeList: " << AttributeList << std::endl;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    }

VOID InstNtCreateThreadEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT  rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT ThreadHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes, ADDRINT ProcessHandle,
    ADDRINT StartAddress, ADDRINT Parameter, ADDRINT CreateFlags, ADDRINT ZeroBits,
    ADDRINT StackSize, ADDRINT MaximumStackSize, ADDRINT AttributeList) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar fun��o
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        IMG img = IMG_FindByAddress(instAddress);
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Return Value: " << retValue << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        // Cria evento
        ExecutionEventData executionEvent(executionCompletedInfo);
        // Notifica os observers
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<InstNtCreateThreadExArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstNtCreateThreadEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

   std::string rtnName = RTN_Name(rtn);
    if (rtnName == "NtCreateThreadEx" || rtnName == "ZwCreateThreadEx") {
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // StartAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // Parameter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // CreateFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ZeroBits
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // StackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // MaximumStackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // AttributeList
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ThreadHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // StartAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // Parameter
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // CreateFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // ZeroBits
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // StackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9, // MaximumStackSize
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10, // AttributeList
            IARG_END);

        RTN_Close(rtn);
    }
}
