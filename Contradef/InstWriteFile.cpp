#include "InstWriteFile.h"

CallContextMap InstWriteFile::callContextMap;
UINT32 InstWriteFile::imgCallId = 0;
CallSequenceNumber InstWriteFile::fcnCallId = 0;
Notifier* InstWriteFile::globalNotifierPtr;

namespace GetHandleInfo {
    // Definições necessárias para NtQueryObject
    typedef enum _OBJECT_INFORMATION_CLASS {
        ObjectBasicInformation,
        ObjectNameInformation,
        ObjectTypeInformation,
        ObjectAllInformation,
        ObjectDataInformation
    } OBJECT_INFORMATION_CLASS;

    typedef struct _UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR Buffer;
    } UNICODE_STRING, * PUNICODE_STRING;

    typedef struct _OBJECT_TYPE_INFORMATION {
        UNICODE_STRING TypeName;
        ULONG TotalNumberOfHandles;
        ULONG TotalNumberOfObjects;
    } OBJECT_TYPE_INFORMATION, * POBJECT_TYPE_INFORMATION;

    typedef NTSTATUS(NTAPI* NtQueryObjectType)(
        HANDLE Handle,
        OBJECT_INFORMATION_CLASS ObjectInformationClass,
        PVOID ObjectInformation,
        ULONG ObjectInformationLength,
        PULONG ReturnLength
        );

    std::string GetHandleDetails(HANDLE h) {
        using namespace WindowsAPI;
        HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
        if (!hNtDll) {
            return "Failed to load ntdll.dll";
        }

        NtQueryObjectType NtQueryObject = (NtQueryObjectType)GetProcAddress(hNtDll, "NtQueryObject");
        if (!NtQueryObject) {
            return "Failed to find NtQueryObject";
        }

        BYTE buffer[1024];
        ULONG returnLength;

        // Obter o tipo do objeto
        WindowsAPI::NTSTATUS status = NtQueryObject(h, ObjectTypeInformation, buffer, sizeof(buffer), &returnLength);
        if (status != 0) {
            return "Failed to query object type information";
        }

        POBJECT_TYPE_INFORMATION objectTypeInfo = (POBJECT_TYPE_INFORMATION)buffer;
        std::wstring wTypeName(objectTypeInfo->TypeName.Buffer, objectTypeInfo->TypeName.Length / sizeof(WCHAR));
        std::string hType = std::string(wTypeName.begin(), wTypeName.end());

        // Obter o nome do objeto
        status = NtQueryObject(h, ObjectNameInformation, buffer, sizeof(buffer), &returnLength);
        if (status != 0) {
            return hType + " (no name available)";
        }

        PUNICODE_STRING objectName = (PUNICODE_STRING)buffer;
        std::string hName;
        if (objectName->Length > 0) {
            std::wstring wName(objectName->Buffer, objectName->Length / sizeof(WCHAR));
            hName = std::string(wName.begin(), wName.end());
        }
        else {
            hName = "Unnamed object";
        }

        // Combinar tipo e nome
        return hType + "; " + hName;
    }
}

VOID InstWriteFile::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT hFile, ADDRINT lpBuffer, ADDRINT nNumberOfBytesToWrite, ADDRINT lpNumberOfBytesWritten, ADDRINT lpOverlapped) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    WriteFileArgs* args = new WriteFileArgs;
    args->hFile = hFile;
    args->lpBuffer = lpBuffer;
    args->nNumberOfBytesToWrite = nNumberOfBytesToWrite;
    args->lpNumberOfBytesWritten = lpNumberOfBytesWritten;
    args->lpOverlapped = lpOverlapped;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);
    
    std::string handleType = GetHandleInfo::GetHandleDetails((WindowsAPI::HANDLE)hFile);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] WriteFile..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Handle Type: " << handleType << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        hFile: 0x" << std::hex << hFile << std::dec << std::endl;
    stringStream << "        lpBuffer: 0x" << std::hex << lpBuffer << std::dec << std::endl;
    stringStream << "        nNumberOfBytesToWrite: " << nNumberOfBytesToWrite << " bytes" << std::endl;
    stringStream << "        lpNumberOfBytesWritten: 0x" << std::hex << lpNumberOfBytesWritten << std::dec << std::endl;
    stringStream << "        lpOverlapped: 0x" << std::hex << lpOverlapped << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call WriteFile" << std::endl;

    size_t bufferSize = nNumberOfBytesToWrite * sizeof(CHAR);
    CHAR* buffer = new CHAR[nNumberOfBytesToWrite + 1];
    memset(buffer, 0, nNumberOfBytesToWrite + 1);
    PIN_SafeCopy(buffer, reinterpret_cast<CHAR*>(lpBuffer), bufferSize);
    std::string bufferStr(buffer);
    stringStream << "    lpBuffer (str): " << bufferStr << std::endl;
    delete[] buffer;
}

VOID InstWriteFile::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT hFile, ADDRINT lpBuffer, ADDRINT nNumberOfBytesToWrite, ADDRINT lpNumberOfBytesWritten, ADDRINT lpOverlapped) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        BOOL result = static_cast<BOOL>(retValue);
        stringStream << "    Returned from WriteFile: " << (result ? "TRUE" : "FALSE") << std::endl;

        if (result) {
            // Sucesso: tenta ler o número de bytes escritos
            if (lpNumberOfBytesWritten != 0) {
                DWORD bytesWritten = 0;
                if (PIN_SafeCopy(&bytesWritten, reinterpret_cast<DWORD*>(lpNumberOfBytesWritten), sizeof(DWORD)) == sizeof(DWORD)) {
                    stringStream << "    Bytes written: " << bytesWritten << " bytes" << std::endl;
                }
                else {
                    stringStream << "    Could not read the number of bytes written." << std::endl;
                }
            }
            else {
                stringStream << "    lpNumberOfBytesWritten is NULL." << std::endl;
            }
        }
        else {
            // Falha
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Operation failed. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from WriteFile completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<WriteFileArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstWriteFile::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "WriteFile") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // BOOL WriteFile(
        //   HANDLE       hFile,
        //   LPCVOID      lpBuffer,
        //   DWORD        nNumberOfBytesToWrite,
        //   LPDWORD      lpNumberOfBytesWritten,
        //   LPOVERLAPPED lpOverlapped
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nNumberOfBytesToWrite
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpNumberOfBytesWritten
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpOverlapped
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // BOOL retorno
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hFile
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // nNumberOfBytesToWrite
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpNumberOfBytesWritten
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpOverlapped
            IARG_END);

        RTN_Close(rtn);
    }
}
