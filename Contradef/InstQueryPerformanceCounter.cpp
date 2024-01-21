#include "InstQueryPerformanceCounter.h"

std::map<CallContextKey, InstQueryPerformanceCounterArgs> InstQueryPerformanceCounter::argsMap;
ADDRINT InstQueryPerformanceCounter::nextCallId = 0;

VOID InstQueryPerformanceCounter::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpPerformanceCount) {
    InstQueryPerformanceCounterArgs args;
    args.lpPerformanceCount = lpPerformanceCount;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
    //std::cout << "[Contradef] QueryPerformanceCounter foi chamado com lpPerformanceCount (Endereço): " << args.lpPerformanceCount << std::endl;

}

VOID InstQueryPerformanceCounter::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpPerformanceCount) {
    //Este callback não está sendo chamado, verificar o motivo ou tentar uma abordagem baseada na leitura do endereço de memoria no modulo principal da aplicação 
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstQueryPerformanceCounterArgs& args = it->second;
        std::string ansiStringlpPerformanceCount = ConvertAddrToAnsiString(args.lpPerformanceCount);

        std::cout << "[Contradef] QueryPerformanceCounter foi chamado com lpPerformanceCount: " << ansiStringlpPerformanceCount << std::endl;
        std::cout << "[Contradef] Valor de retorno de QueryPerformanceCounter: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstQueryPerformanceCounter::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpPerformanceCount
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpPerformanceCount
        IARG_END);

    RTN_Close(rtn);

}