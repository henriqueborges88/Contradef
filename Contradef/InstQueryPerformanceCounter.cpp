#include "InstQueryPerformanceCounter.h"

std::map<CallContextKey, InstQueryPerformanceCounterArgs> InstQueryPerformanceCounter::argsMap;
ADDRINT InstQueryPerformanceCounter::nextCallId = 0;
std::ofstream* InstQueryPerformanceCounter::outStream;

VOID InstQueryPerformanceCounter::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpPerformanceCount) {
    
    InstQueryPerformanceCounterArgs args;
    args.lpPerformanceCount = lpPerformanceCount;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;

}

VOID InstQueryPerformanceCounter::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpPerformanceCount) {
    //Este callback não está sendo chamado, verificar o motivo ou tentar uma abordagem baseada na leitura do endereço de memoria no modulo principal da aplicação 
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstQueryPerformanceCounterArgs& args = it->second;
        std::string ansiStringlpPerformanceCount = ConvertAddrToAnsiString(args.lpPerformanceCount);
        *outStream << std::endl << "[+] QueryPerformanceCounter..." << std::endl;
        *outStream << "    lpPerformanceCount (parámetro): " << ansiStringlpPerformanceCount << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
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