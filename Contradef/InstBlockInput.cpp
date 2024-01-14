#include "InstBlockInput.h"

/*
A função BlockInput possui um argumento, fBlockIt, que é um valor booleano indicando se a entrada do usuário deve ser bloqueada ou desbloqueada. O método CallbackBefore registra este parâmetro, e o CallbackAfter registra o valor de retorno.
*/

std::map<ADDRINT, InstBlockInputArgs> InstBlockInput::argsMap;

VOID InstBlockInput::CallbackBefore(ADDRINT instrPtr, ADDRINT fBlockIt) {
    InstBlockInputArgs args;
    args.fBlockIt = fBlockIt;

    argsMap[instrPtr] = args;

    std::cout << "BlockInput será chamada com o seguinte parâmetro:" << std::endl;
    std::cout << "fBlockIt: " << fBlockIt << std::endl;
}

VOID InstBlockInput::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT fBlockIt) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        std::cout << "Valor de retorno de BlockInput: " << *retValAddr << std::endl;
    }

    std::cout << "BlockInput completou a execução." << std::endl;
}

VOID InstBlockInput::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);
    
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // fBlockIt
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // fBlockIt
        IARG_END);

    RTN_Close(rtn);

}
