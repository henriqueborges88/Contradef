#include "InstGetAsyncKeyState.h"

/*
A função GetAsyncKeyState possui um argumento, vKey, que é um valor inteiro representando a tecla virtual a ser verificada. O método CallbackBefore registra este parâmetro, e o CallbackAfter registra o valor de retorno.
*/

std::map<ADDRINT, InstGetAsyncKeyStateArgs> InstGetAsyncKeyState::argsMap;

VOID InstGetAsyncKeyState::CallbackBefore(ADDRINT instrPtr, ADDRINT vKey) {
    InstGetAsyncKeyStateArgs args;
    args.vKey = vKey;

    argsMap[instrPtr] = args;

    std::cout << "GetAsyncKeyState será chamada com o seguinte parâmetro:" << std::endl;
    std::cout << "vKey: " << vKey << std::endl;
}

VOID InstGetAsyncKeyState::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT vKey) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        std::cout << "Valor de retorno de GetAsyncKeyState: " << *retValAddr << std::endl;
    }

    std::cout << "GetAsyncKeyState completou a execução." << std::endl;
}

VOID InstGetAsyncKeyState::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // vKey
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // vKey
        IARG_END);

    RTN_Close(rtn);

}
