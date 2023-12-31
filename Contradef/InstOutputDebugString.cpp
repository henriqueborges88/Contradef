#include "InstOutputDebugString.h"

/*
A função OutputDebugString possui um único argumento, lpOutputString, que é um ponteiro para a string a ser enviada para o depurador. O método CallbackBefore registra este parâmetro, e o CallbackAfter indica a conclusão da execução da função.
*/

std::map<ADDRINT, InstOutputDebugStringArgs> InstOutputDebugString::argsMap;

VOID InstOutputDebugString::CallbackBefore(ADDRINT instrPtr, ADDRINT lpOutputString) {
    InstOutputDebugStringArgs args;
    args.lpOutputString = lpOutputString;

    argsMap[instrPtr] = args;

    std::cout << "OutputDebugString será chamada com o seguinte parâmetro:" << std::endl;
    std::cout << "lpOutputString (Endereço): " << lpOutputString << std::endl;
}

VOID InstOutputDebugString::CallbackAfter(ADDRINT instrPtr, ADDRINT lpOutputString) {
    std::cout << "OutputDebugString completou a execução." << std::endl;
}

VOID InstOutputDebugString::InstrumentFunction(RTN rtn) {
    PROTO protoOutputDebugString = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_STDCALL,
        "OutputDebugString",
        PIN_PARG(ADDRINT), // lpOutputString
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoOutputDebugString,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
        IARG_END);

    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoOutputDebugString,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpOutputString
        IARG_END);

    PROTO_Free(protoOutputDebugString);
}
