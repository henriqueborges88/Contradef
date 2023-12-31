#include "InstCheckRemoteDebuggerPresent.h"

/*
A função CheckRemoteDebuggerPresent possui dois argumentos: um handle para um processo (hProcess) e um ponteiro para uma variável que receberá a indicação de presença do depurador (pbDebuggerPresent). O método CallbackBefore registra esses parâmetros, enquanto o CallbackAfter registra o valor de retorno.
*/

std::map<ADDRINT, InstCheckRemoteDebuggerPresentArgs> InstCheckRemoteDebuggerPresent::argsMap;

VOID InstCheckRemoteDebuggerPresent::CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {
    InstCheckRemoteDebuggerPresentArgs args;
    args.hProcess = hProcess;
    args.pbDebuggerPresent = pbDebuggerPresent;

    argsMap[instrPtr] = args;

    std::cout << "[Contradef] CheckRemoteDebuggerPresent será chamada com os seguintes parâmetros:" << std::endl;
    std::cout << "[Contradef] hProcess (Endereço): " << hProcess << std::endl;
    std::cout << "[Contradef] pbDebuggerPresent (Endereço): " << pbDebuggerPresent << std::endl;
}

VOID InstCheckRemoteDebuggerPresent::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT pbDebuggerPresent) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const InstCheckRemoteDebuggerPresentArgs& args = it->second;
        std::cout << "[Contradef] Valor de retorno de CheckRemoteDebuggerPresent: " << *retValAddr << std::endl;
    }

    std::cout << "[Contradef] CheckRemoteDebuggerPresent completou a execução." << std::endl;
}

VOID InstCheckRemoteDebuggerPresent::InstrumentFunction(RTN rtn) {
    PROTO protoCheckRemoteDebuggerPresent = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "CheckRemoteDebuggerPresent",
        PIN_PARG(ADDRINT), // hProcess
        PIN_PARG(ADDRINT), // pbDebuggerPresent
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoCheckRemoteDebuggerPresent,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoCheckRemoteDebuggerPresent,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // pbDebuggerPresent
        IARG_END);

    PROTO_Free(protoCheckRemoteDebuggerPresent);
}
