#include "InstCompareStringEx.h"

CallContextMap InstCompareStringEx::callContextMap;
UINT32 InstCompareStringEx::imgCallId = 0;
CallSequenceNumber InstCompareStringEx::fcnCallId = 0;
Notifier* InstCompareStringEx::globalNotifierPtr;

VOID InstCompareStringEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpLocaleName, ADDRINT dwCmpFlags, ADDRINT lpString1, ADDRINT cchCount1,
    ADDRINT lpString2, ADDRINT cchCount2, ADDRINT lpVersionInformation, ADDRINT lpReserved, ADDRINT sortHandle) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        //return;
    }

    CompareStringExArgs* args = new CompareStringExArgs;
    args->lpLocaleName = lpLocaleName;
    args->dwCmpFlags = dwCmpFlags;
    args->lpString1 = lpString1;
    args->cchCount1 = cchCount1;
    args->lpString2 = lpString2;
    args->cchCount2 = cchCount2;
    args->lpVersionInformation = lpVersionInformation;
    args->lpReserved = lpReserved;
    args->sortHandle = sortHandle;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] CompareStringEx..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;

    // Leitura do locale name
    if (lpLocaleName != 0) {
        std::wstring localeName;
        localeName.resize(256);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&localeName[0], reinterpret_cast<wchar_t*>(lpLocaleName), 255 * sizeof(wchar_t)) / sizeof(wchar_t);
        localeName[charsRead] = L'\0';
        stringStream << "        lpLocaleName: " << WStringToString(localeName) << std::endl;
    }
    else {
        stringStream << "        lpLocaleName: NULL" << std::endl;
    }

    stringStream << "        dwCmpFlags: 0x" << std::hex << dwCmpFlags << std::dec << std::endl;

    // String 1
    if (lpString1 != 0 && cchCount1 != 0) {
        int charsToRead = (cchCount1 > 0 && cchCount1 < 256) ? cchCount1 : 256;
        std::wstring string1;
        string1.resize(charsToRead);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&string1[0], reinterpret_cast<wchar_t*>(lpString1), (charsToRead - 1) * sizeof(wchar_t)) / sizeof(wchar_t);
        string1[charsRead] = L'\0';
        stringStream << "        lpString1: " << WStringToString(string1) << std::endl;
    }
    else {
        stringStream << "        lpString1: NULL" << std::endl;
    }

    // String 2
    if (lpString2 != 0 && cchCount2 != 0) {
        int charsToRead = (cchCount2 > 0 && cchCount2 < 256) ? cchCount2 : 256;
        std::wstring string2;
        string2.resize(charsToRead);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&string2[0], reinterpret_cast<wchar_t*>(lpString2), (charsToRead - 1) * sizeof(wchar_t)) / sizeof(wchar_t);
        string2[charsRead] = L'\0';
        stringStream << "        lpString2: " << WStringToString(string2) << std::endl;
    }
    else {
        stringStream << "        lpString2: NULL" << std::endl;
    }

    stringStream << "        lpVersionInformation: 0x" << std::hex << lpVersionInformation << std::dec << std::endl;
    stringStream << "        lpReserved: 0x" << std::hex << lpReserved << std::dec << std::endl;
    stringStream << "        sortHandle: 0x" << std::hex << sortHandle << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call CompareStringEx" << std::endl;

}

VOID InstCompareStringEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT lpLocaleName, ADDRINT dwCmpFlags, ADDRINT lpString1, ADDRINT cchCount1,
    ADDRINT lpString2, ADDRINT cchCount2, ADDRINT lpVersionInformation, ADDRINT lpReserved, ADDRINT sortHandle) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        //return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        int result = static_cast<int>(retValue);
        stringStream << "    Returned from CompareStringEx: " << result << std::endl;

        if (result > 0) {
            stringStream << "    Operation successful." << std::endl;
        }
        else {
            stringStream << "    Operation failed." << std::endl;
        }

        stringStream << "  [-] Call from CompareStringEx completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionCompleted(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionCompleted);

        const CompareStringExArgs* args = reinterpret_cast<CompareStringExArgs*>(callContext->functionArgs);

        delete args;
        delete callContext;
    }

    fcnCallId++;
}

VOID InstCompareStringEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CompareStringEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // int CompareStringEx(
        //   LPCWSTR          lpLocaleName,
        //   DWORD            dwCmpFlags,
        //   LPCWSTR          lpString1,
        //   int              cchCount1,
        //   LPCWSTR          lpString2,
        //   int              cchCount2,
        //   LPNLSVERSIONINFO lpVersionInformation,
        //   LPVOID           lpReserved,
        //   LPARAM           sortHandle
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLocaleName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwCmpFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpString1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // cchCount1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpString2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // cchCount2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpVersionInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // sortHandle
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // int retorno
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLocaleName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwCmpFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpString1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // cchCount1
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpString2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // cchCount2
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpVersionInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // sortHandle
            IARG_END);

        RTN_Close(rtn);
    }
}
