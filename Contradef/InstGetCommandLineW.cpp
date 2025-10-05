#include "InstGetCommandLineW.h"

std::map<CallContextKey, CallContext*> InstGetCommandLineW::callContextMap;
UINT32 InstGetCommandLineW::imgCallId = 0;
UINT32 InstGetCommandLineW::fcnCallId = 0;
Notifier* InstGetCommandLineW::globalNotifierPtr;

VOID InstGetCommandLineW::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, nullptr);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetCommandLineW..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: (none)" << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetCommandLineW" << std::endl;

}

VOID InstGetCommandLineW::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress, ADDRINT retValue) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        CallContext* callContext = it->second;
        std::stringstream& stringStream = callContext->stringStream;

        // Valor de retorno é um LPWSTR (wchar_t*)
        wchar_t* cmdLine = reinterpret_cast<wchar_t*>(retValue);
        if (cmdLine != nullptr) {
            // Limita a leitura a 256 caracteres
            std::wstring cmdLineWstr;
            cmdLineWstr.resize(256);
            WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(&cmdLineWstr[0], cmdLine, 255 * sizeof(wchar_t)) / sizeof(wchar_t);
            cmdLineWstr[charsRead] = L'\0';

            stringStream << "    Returned from GetCommandLineW: 0x" << std::hex << retValue << std::dec << " ("
                << WStringToString(cmdLineWstr) << ")" << std::endl;
        }
        else {
            stringStream << "    Returned from GetCommandLineW: NULL" << std::endl;
        }

        stringStream << "    Complete operation." << std::endl;
        stringStream << "  [-] Call from GetCommandLineW completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstGetCommandLineW::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetCommandLineW") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // LPWSTR GetCommandLineW(void);
        //
        // Retorna um LPWSTR (ponteiro para string wide da linha de comando).

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (LPWSTR)
            IARG_END);

        RTN_Close(rtn);
    }
}
