#include "InstNtQuerySystemInformation.h"

/*
https://anti-debug.checkpoint.com/techniques/debug-flags.html#using-win32-api-checks-ntquerysysteminformation
A função NtQuerySystemInformation possui quatro argumentos: SystemInformationClass, SystemInformation, SystemInformationLength e ReturnLength. O método CallbackBefore registra esses parâmetros, e o CallbackAfter registra o valor de retorno.
*/
std::map<ADDRINT, InstNtQuerySystemInformationArgs> InstNtQuerySystemInformation::argsMap;

VOID InstNtQuerySystemInformation::CallbackBefore(ADDRINT instrPtr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength) {
    InstNtQuerySystemInformationArgs args;
    args.SystemInformationClass = SystemInformationClass;
    args.SystemInformation = SystemInformation;
    args.SystemInformationLength = SystemInformationLength;
    args.ReturnLength = ReturnLength;

    argsMap[instrPtr] = args;

    std::cout << "[Contradef] NtQuerySystemInformation será chamada com os seguintes parâmetros:" << std::endl;
    // Impressão dos parâmetros
}

VOID InstNtQuerySystemInformation::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "[Contradef] Valor de retorno de NtQuerySystemInformation: " << *retValAddr << std::endl;
}

VOID InstNtQuerySystemInformation::InstrumentFunction(RTN rtn) {
    PROTO protoNtQuerySystemInformation = PROTO_Allocate(PIN_PARG(LONG), CALLINGSTD_STDCALL,
        "NtQuerySystemInformation",
        PIN_PARG(ADDRINT), // SystemInformationClass
        PIN_PARG(ADDRINT), // SystemInformation
        PIN_PARG(ADDRINT), // SystemInformationLength
        PIN_PARG(ADDRINT), // ReturnLength
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoNtQuerySystemInformation,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoNtQuerySystemInformation,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_END);

    PROTO_Free(protoNtQuerySystemInformation);
}
