#include "InstGetComputerNameA.h"

CallContextMap InstGetComputerNameA::callContextMap;
UINT32 InstGetComputerNameA::imgCallId = 0;
CallSequenceNumber InstGetComputerNameA::fcnCallId = 0;
Notifier* InstGetComputerNameA::globalNotifierPtr;

VOID InstGetComputerNameA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
    ADDRINT lpBuffer, ADDRINT nSize) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    GetComputerNameAArgs* args = new GetComputerNameAArgs;
    args->lpBuffer = lpBuffer;
    args->nSize = nSize;

    UINT32 callCtxId = callId * 100 + fcnCallId;
    auto* callContext = new CallContext(callCtxId, tid, instAddress, args);
    CallContextKey key = { callId, tid };
    SaveCallContext(callContextMap, key, callContext);

    std::stringstream& stringStream = callContext->stringStream;
    stringStream << std::endl << "[+] GetComputerNameA..." << std::endl;
    stringStream << "    Thread: " << tid << std::endl;
    stringStream << "    Call Id: " << fcnCallId << std::endl;
    stringStream << "    Routine address: 0x" << std::hex << callContext->rtnAddress << std::dec << std::endl;
    stringStream << "    Parameters: " << std::endl;
    stringStream << "        lpBuffer: 0x" << std::hex << lpBuffer << std::dec << std::endl;
    stringStream << "        nSize (LPDWORD): 0x" << std::hex << nSize << std::dec << std::endl;
    stringStream << "    Address of calling function: 0x" << std::hex << returnAddress << std::dec << std::endl;
    stringStream << "  [-] Start of call GetComputerNameA" << std::endl;

}

VOID InstGetComputerNameA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* returnAddress,
    ADDRINT retValue, ADDRINT lpBuffer, ADDRINT nSize) {

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
        stringStream << "    Returned from GetComputerNameA: " << (result ? "TRUE" : "FALSE") << std::endl;

        if (result != 0) {
            // Sucesso: tenta ler o nome do computador
            if (lpBuffer != 0 && nSize != 0) {
                DWORD bufferSize = 0;
                PIN_SafeCopy(&bufferSize, reinterpret_cast<DWORD*>(nSize), sizeof(DWORD));
                if (bufferSize > 0) {
                    // Limita a leitura a um tamanho razoável
                    DWORD bytesToRead = (bufferSize < 256) ? bufferSize : 256;
                    std::vector<char> compName(bytesToRead, '\0');
                    WindowsAPI::SIZE_T charsRead = PIN_SafeCopy(compName.begin(), reinterpret_cast<char*>(lpBuffer), bytesToRead - 1);
                    compName[charsRead] = '\0';

                    stringStream << "    Computer name: " << compName.begin() << std::endl;
                }
                else {
                    stringStream << "    nSize points to zero or could not be read." << std::endl;
                }
            }
            else {
                stringStream << "    lpBuffer NULL or invalid nSize. Could not read computer name." << std::endl;
            }
            stringStream << "    Operation completed successfully." << std::endl;
        }
        else {
            using namespace WindowsAPI;
            DWORD error = GetLastError();
            stringStream << "    Failed to get computer name. Error code: " << error << std::endl;
        }

        stringStream << "  [-] Call from GetComputerNameA completed" << std::endl;
        stringStream << "[*] Completed" << std::endl << std::endl;

        ExecutionInformation executionCompletedInfo = { stringStream.str() };
        ExecutionEventData executionEvent(executionCompletedInfo);
        globalNotifierPtr->NotifyAll(&executionEvent);

        delete reinterpret_cast<GetComputerNameAArgs*>(callContext->functionArgs);
        delete callContext;
    }

    fcnCallId++;
}

VOID InstGetComputerNameA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetComputerNameA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // Assinatura:
        // BOOL GetComputerNameA(
        //   LPSTR  lpBuffer,
        //   LPDWORD nSize
        // );

        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallbackBefore,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // nSize
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallbackAfter,
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_REG_VALUE, REG_STACK_PTR, // Endereço de retorno (funçao chamante) 
            IARG_FUNCRET_EXITPOINT_VALUE,    // valor de retorno (BOOL)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpBuffer
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // nSize
            IARG_END);

        RTN_Close(rtn);
    }
}
