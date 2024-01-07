#include "InstQueryPerformanceCounter.h"

/*
A função QueryPerformanceCounter possui um argumento, lpPerformanceCount, que é um ponteiro para uma estrutura que recebe o valor do contador de desempenho. O método CallbackBefore registra este parâmetro, e o CallbackAfter registra o valor de retorno e indica a conclusão da execução da função.
*/

std::map<ADDRINT, InstQueryPerformanceCounterArgs> InstQueryPerformanceCounter::argsMap;

VOID InstQueryPerformanceCounter::CallbackBefore(ADDRINT instrPtr, ADDRINT lpPerformanceCount) {
    InstQueryPerformanceCounterArgs args;
    args.lpPerformanceCount = lpPerformanceCount;

    argsMap[instrPtr] = args;

    std::cout << "[Contradef] QueryPerformanceCounter será chamada com lpPerformanceCount (Endereço): " << lpPerformanceCount << std::endl;
}

VOID InstQueryPerformanceCounter::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpPerformanceCount) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const InstQueryPerformanceCounterArgs& args = it->second;
        std::cout << "Valor de retorno de QueryPerformanceCounter: " << *retValAddr << std::endl;
    }

    std::cout << "QueryPerformanceCounter completou a execução." << std::endl;
}

VOID InstQueryPerformanceCounter::InstrumentFunction(RTN rtn) {
    PROTO protoQueryPerformanceCounter = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "QueryPerformanceCounter",
        PIN_PARG(ADDRINT), // lpPerformanceCount
        PIN_PARG_END());

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoQueryPerformanceCounter,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpPerformanceCount
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoQueryPerformanceCounter,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpPerformanceCount
        IARG_END);

    RTN_Close(rtn);

    PROTO_Free(protoQueryPerformanceCounter);
}
