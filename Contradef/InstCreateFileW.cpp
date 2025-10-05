#include "InstCreateFileW.h"

std::map<CallContextKey, CallContext*> InstCreateFileW::callContextMap;
UINT32 InstCreateFileW::imgCallId = 0;
UINT32 InstCreateFileW::fcnCallId = 0;
Notifier* InstCreateFileW::globalNotifierPtr;

VOID InstCreateFileW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpFileName, ADDRINT dwDesiredAccess, ADDRINT dwShareMode, ADDRINT lpSecurityAttributes, ADDRINT dwCreationDisposition, ADDRINT dwFlagsAndAttributes, ADDRINT hTemplateFile) {

    if (IsStringInStrategyScope("Obsidium")) {
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            if (!IsFromUnknownImg(returnAddress))
                return;
        }
    }
    else {
        if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
            return;
        }
    }

    CreateFileWArgs* args = new CreateFileWArgs;
    args->lpFileName = ConvertAddrToWideStringSafe(lpFileName);
    args->dwDesiredAccess = dwDesiredAccess;
    args->dwShareMode = dwShareMode;
    args->lpSecurityAttributes = lpSecurityAttributes;
    args->dwCreationDisposition = dwCreationDisposition;
    args->dwFlagsAndAttributes = dwFlagsAndAttributes;
    args->hTemplateFile = hTemplateFile;
    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Registrando os par�metros de CreateFileW e o endere�o da fun��o chamante
    std::stringstream& stringStream = callContext->stringStream;
    std::wstring wsFileName = ConvertAddrToWideString(lpFileName);
    stringStream << std::endl << "[+] CreateFileW..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpFileName: " << WStringToString(wsFileName) << std::endl;
    stringStream << "        dwDesiredAccess: " << dwDesiredAccess << std::endl;
    stringStream << "        dwShareMode: " << dwShareMode << std::endl;
    stringStream << "        dwCreationDisposition: " << dwCreationDisposition << std::endl;
    stringStream << "        dwFlagsAndAttributes: " << dwFlagsAndAttributes << std::endl;
    stringStream << "    Address of calling function: " << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of file access" << std::endl;

}

VOID InstCreateFileW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* retValueRef, ADDRINT* returnAddress, ADDRINT lpFileName) {
    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        const CreateFileWArgs* args = reinterpret_cast<CreateFileWArgs*>(callContext->functionArgs);
        std::stringstream& stringStream = callContext->stringStream;

        stringStream << "    Returned Handle: " << std::hex << *(HANDLE*)retValueRef << std::dec << std::endl;
        stringStream << "  [-] File access completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        // CONTRAMEDIDA
        if (args->lpFileName == L"\\\\.\\TitanHide" || args->lpFileName == L"\\\\.\\VBoxGuest")
        {
            HANDLE real = (HANDLE)(*retValueRef);   // valor retornado
            if (real && real != INVALID_HANDLE_VALUE) {
                WindowsAPI::CloseHandle(real);           // evita vazamento
            }
            *retValueRef = (ADDRINT)INVALID_HANDLE_VALUE;     // força erro
            WindowsAPI::SetLastError(ERROR_FILE_NOT_FOUND);        // erro coerente
        }

        delete reinterpret_cast<CreateFileWArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstCreateFileW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateFileW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        if (IsStringInStrategyScope("Obsidium")) {
            // fallback para o Obsidium
            size_t scanSize = 0xf;
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
                        IARG_RETURN_IP,                        // Endere�o da fun��o chamante
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // lpFileName
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // dwDesiredAccess
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // dwShareMode
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // lpSecurityAttributes
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 4,      // dwCreationDisposition
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 5,      // dwFlagsAndAttributes
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 6,      // hTemplateFile
                        IARG_END);
                }
            }
        }

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                        // Endere�o da fun��o chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // lpFileName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,      // dwDesiredAccess
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,      // dwShareMode
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,      // lpSecurityAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,      // dwCreationDisposition
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,      // dwFlagsAndAttributes
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6,      // hTemplateFile
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_REFERENCE,           // Handle retornado
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,      // lpFileName
            IARG_END);

        RTN_Close(rtn);
    }
}
