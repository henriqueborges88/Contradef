#include "InstVirtualQueryEx.h"

CallContextMap InstVirtualQueryEx::callContextMap;
UINT32 InstVirtualQueryEx::imgCallId = 0;
CallSequenceNumber InstVirtualQueryEx::fcnCallId = 0;
Notifier* InstVirtualQueryEx::globalNotifierPtr;


// Baseado em https://stackoverflow.com/questions/65897402/c-winapi-virtualqueryex-function-gives-me-000000
unsigned long InstVirtualQueryEx::show_module(WindowsAPI::MEMORY_BASIC_INFORMATION info, std::stringstream& stringStream) {
    unsigned long usage = 0;

    stringStream << "    Memory block information: " << info.BaseAddress << "(" << info.RegionSize / 1024 << ")\t";

    switch (info.State) {
    case MEM_COMMIT:
        stringStream << "Committed";
        break;
    case MEM_RESERVE:
        stringStream << "Reserved";
        break;
    case MEM_FREE:
        stringStream << "Free";
        break;
    }
    stringStream << "\t";
    switch (info.Type) {
    case MEM_IMAGE:
        stringStream << "Code Module";
        break;
    case MEM_MAPPED:
        stringStream << "Mapped     ";
        break;
    case MEM_PRIVATE:
        stringStream << "Private    ";
    }
    stringStream << "\t";

    int guard = 0, nocache = 0;

    if (info.AllocationProtect & PAGE_NOCACHE)
        nocache = 1;
    if (info.AllocationProtect & PAGE_GUARD)
        guard = 1;

    info.AllocationProtect &= ~(PAGE_GUARD | PAGE_NOCACHE);

    if ((info.State == MEM_COMMIT) && (info.AllocationProtect == PAGE_READWRITE || info.AllocationProtect == PAGE_READONLY))
        usage += info.RegionSize;

    switch (info.AllocationProtect) {
    case PAGE_READONLY:
        stringStream << "Read Only";
        break;
    case PAGE_READWRITE:
        stringStream << "Read/Write";
        break;
    case PAGE_WRITECOPY:
        stringStream << "Copy on Write";
        break;
    case PAGE_EXECUTE:
        stringStream << "Execute only";
        break;
    case PAGE_EXECUTE_READ:
        stringStream << "Execute/Read";
        break;
    case PAGE_EXECUTE_READWRITE:
        stringStream << "Execute/Read/Write";
        break;
    case PAGE_EXECUTE_WRITECOPY:
        stringStream << "COW Executable";
        break;
    }

    if (guard)
        stringStream << "\tguard page";
    if (nocache)
        stringStream << "\tnon-cacheable";
    stringStream << "\n";
    return usage;
}

VOID InstVirtualQueryEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    InstVirtualQueryExArgs* args = new InstVirtualQueryExArgs;
    args->hProcess = hProcess;
    args->lpAddress = lpAddress;
    args->lpBuffer = lpBuffer;
    args->dwLength = dwLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);

    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);
}

VOID InstVirtualQueryEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT retValue, ADDRINT* returnAddress,
    ADDRINT hProcess, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    // Instrumentar função
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = pendingCallContext;
        // Registrar parâmetros
        const InstVirtualQueryExArgs* args = reinterpret_cast<InstVirtualQueryExArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        // Obter a RTN da instrução atual
        stringStream << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        stringStream << "    Module name: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Call Id: " << fcnCallId << std::endl;
        stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
        stringStream << "    Parameters: " << std::endl;
        stringStream << "        hProcess: " << args->hProcess << std::endl;
        stringStream << "        lpAddress: " << args->lpAddress << std::endl;
        stringStream << "        lpBuffer: " << args->lpBuffer << std::endl;
        stringStream << "        dwLength: " << args->dwLength << std::endl;
        stringStream << "    Return Value: " << retValue << std::endl;
        WindowsAPI::MEMORY_BASIC_INFORMATION* info = reinterpret_cast<WindowsAPI::MEMORY_BASIC_INFORMATION*>(args->lpBuffer);
        show_module(*info, stringStream);
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

VOID InstVirtualQueryEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "VirtualQueryEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // dwLength
            IARG_END);

        RTN_Close(rtn);
    }
}