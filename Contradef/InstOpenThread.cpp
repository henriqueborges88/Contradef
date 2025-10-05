#include "InstOpenThread.h"
#include "HeapTracker.h"
#include "SectionTracker.h"

std::map<CallContextKey, CallContext*> InstOpenThread::callContextMap;
UINT32 InstOpenThread::imgCallId = 0;
UINT32 InstOpenThread::fcnCallId = 0;
Notifier* InstOpenThread::globalNotifierPtr;

VOID InstOpenThread::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT dwDesiredAccess, ADDRINT bInheritHandle, ADDRINT dwThreadId) {
    
    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            return;
        }
    }

    OpenThreadArgs* args = new OpenThreadArgs;
    args->dwDesiredAccess = dwDesiredAccess;
    args->bInheritHandle = bInheritHandle;
    args->dwThreadId = dwThreadId;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os parâmetros e o endereço da função chamante
    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] OpenThread..." << std::endl;
    stringStream << "    Thread ID: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << rtn << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        dwDesiredAccess: " << dwDesiredAccess << std::endl;
    stringStream << "        bInheritHandle: " << bInheritHandle << std::endl;
    stringStream << "        dwThreadId: " << dwThreadId << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call OpenThread" << std::endl;

}

VOID InstOpenThread::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retVal) {

    if (!IsStringInStrategyScope("Obsidium")) {
        // Não é necessário para o Obsidium
        if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
            return;
        }
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;

    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Return Value (Thread Handle): " << std::hex << retVal << std::dec << std::endl;
        stringStream << "  [-] Call from OpenThread completed" << std::endl;
        stringStream << "[*] Completed" << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        const OpenThreadArgs* args = reinterpret_cast<OpenThreadArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstOpenThread::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "OpenThread") {
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

        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);
        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0x11;
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
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // dwDesiredAccess
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // bInheritHandle
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // dwThreadId
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
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // dwDesiredAccess
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // bInheritHandle
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // dwThreadId
                IARG_END);
        }

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,          // Valor de retorno (HANDLE)
            IARG_END);

        RTN_Close(rtn);
    }
}
