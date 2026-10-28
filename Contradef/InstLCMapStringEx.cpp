#include "InstLCMapStringEx.h"

CallContextMap InstLCMapStringEx::callContextMap;
UINT32 InstLCMapStringEx::imgCallId = 0;
CallSequenceNumber InstLCMapStringEx::fcnCallId = 0;
Notifier* InstLCMapStringEx::globalNotifierPtr;

VOID InstLCMapStringEx::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpLocaleName, ADDRINT dwMapFlags, ADDRINT lpSrcStr, ADDRINT cchSrc,
    ADDRINT lpDestStr, ADDRINT cchDest, ADDRINT lpVersionInformation,
    ADDRINT lpReserved, ADDRINT lParam) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    LCMapStringExArgs* args = new LCMapStringExArgs;
    args->lpLocaleName = lpLocaleName;
    args->dwMapFlags = dwMapFlags;
    args->lpSrcStr = lpSrcStr;
    args->cchSrc = (int)cchSrc;
    args->lpDestStr = lpDestStr;
    args->cchDest = (int)cchDest;
    args->lpVersionInformation = lpVersionInformation;
    args->lpReserved = lpReserved;
    args->lParam = lParam;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] LCMapStringEx..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;

    // Tenta ler a lpLocaleName (LPCWSTR)
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

    stringStream << "        dwMapFlags: 0x" << std::hex << dwMapFlags << std::dec << std::endl;

    // Tenta ler a string de entrada (lpSrcStr) se cchSrc > 0
    if (lpSrcStr != 0 && cchSrc > 0) {
        int charsToRead = (cchSrc < 256) ? cchSrc : 256;
        std::wstring srcStr;
        srcStr.resize(charsToRead);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&srcStr[0], reinterpret_cast<wchar_t*>(lpSrcStr), (charsToRead - 1) * sizeof(wchar_t)) / sizeof(wchar_t);
        srcStr[charsRead] = L'\0';
        stringStream << "        lpSrcStr: " << WStringToString(srcStr) << " (cchSrc: " << cchSrc << ")" << std::endl;
    }
    else if (lpSrcStr != 0 && cchSrc == -1) {
        // String null-terminated
        std::wstring srcStr;
        srcStr.resize(256);
        WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&srcStr[0], reinterpret_cast<wchar_t*>(lpSrcStr), 255 * sizeof(wchar_t)) / sizeof(wchar_t);
        srcStr[charsRead] = L'\0';
        stringStream << "        lpSrcStr: " << WStringToString(srcStr) << " (cchSrc: -1, null-terminated)" << std::endl;
    }
    else {
        stringStream << "        lpSrcStr: NULL or invalid cchSrc" << std::endl;
    }

    stringStream << "        lpDestStr: 0x" << std::hex << lpDestStr << std::dec << ", cchDest: " << cchDest << std::endl;
    stringStream << "        lpVersionInformation: 0x" << std::hex << lpVersionInformation << std::dec << std::endl;
    stringStream << "        lpReserved: 0x" << std::hex << lpReserved << std::dec << std::endl;
    stringStream << "        lParam: 0x" << std::hex << lParam << std::dec << std::endl;

    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call LCMapStringEx" << std::endl;

}

VOID InstLCMapStringEx::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT lpLocaleName, ADDRINT dwMapFlags, ADDRINT lpSrcStr, ADDRINT cchSrc,
    ADDRINT lpDestStr, ADDRINT cchDest, ADDRINT lpVersionInformation, ADDRINT lpReserved, ADDRINT lParam) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callId, tid };
    CallContext* pendingCallContext = TakeCallContext(callContextMap, key);
    if (pendingCallContext != nullptr) {
        CallContext* callContext = pendingCallContext;
        std::stringstream& stringStream = callContext->stringStream;

        int result = static_cast<int>(retValue);
        stringStream << "    Returned from LCMapStringEx: " << result << std::endl;

        if (result > 0) {
            // Sucesso: tenta ler a string de saída
            if (lpDestStr != 0 && cchDest > 0) {
                int charsToRead = (cchDest < 256) ? cchDest : 256;
                std::wstring destStr;
                destStr.resize(charsToRead);
                WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&destStr[0], reinterpret_cast<wchar_t*>(lpDestStr), (charsToRead - 1) * sizeof(wchar_t)) / sizeof(wchar_t);
                destStr[charsRead] = L'\0';

                stringStream << "    Result String (lpDestStr): " << WStringToString(destStr) << std::endl;
            }
            else {
                stringStream << "    lpDestStr NULL or invalid cchDest. Could not read the resulting string." << std::endl;
            }
            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            // Falha
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Operation failed. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from LCMapStringEx completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<LCMapStringExArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstLCMapStringEx::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "LCMapStringEx") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // int LCMapStringEx(
        //   LPCWSTR          lpLocaleName,
        //   DWORD            dwMapFlags,
        //   LPCWSTR          lpSrcStr,
        //   int              cchSrc,
        //   LPWSTR           lpDestStr,
        //   int              cchDest,
        //   LPNLSVERSIONINFO lpVersionInformation,
        //   LPVOID           lpReserved,
        //   LPARAM           lParam
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpLocaleName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwMapFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpSrcStr
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // cchSrc
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDestStr
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // cchDest
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpVersionInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lParam
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
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // dwMapFlags
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpSrcStr
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // cchSrc
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpDestStr
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // cchDest
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // lpVersionInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7, // lpReserved
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8, // lParam
            IARG_END);

        RTN_Close(rtn);
    }
}
