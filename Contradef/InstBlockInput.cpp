#include "InstBlockInput.h"
/*
A função BlockInput na API do Windows é usada para bloquear ou desbloquear a entrada do teclado e do mouse no sistema. Esta função pode ser útil em cenários onde é necessário impedir a interação do usuário por um curto período, como durante uma operação crítica que não deve ser interrompida. No entanto, é importante usar essa função com cautela, pois ela impacta diretamente a capacidade do usuário de interagir com o computador.
Verificar se está relacionada com detecção de Debug ou DBI
*/
std::map<CallContextKey, InstBlockInputArgs> InstBlockInput::argsMap;
ADDRINT InstBlockInput::nextCallId = 0;

VOID InstBlockInput::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT fBlockIt) {
    InstBlockInputArgs args;
    args.fBlockIt = fBlockIt;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstBlockInput::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT fBlockIt) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        long* fBlockItPtr = reinterpret_cast<long*>(it->second.fBlockIt);
        std::cout << "[Contradef] BlockInput foi chamado com fBlockIt: " << *fBlockItPtr << std::endl;
        std::cout << "[Contradef] Valor de retorno de BlockInput: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstBlockInput::InstrumentFunction(RTN rtn) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // fBlockIt
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // fBlockIt
        IARG_END);

    RTN_Close(rtn);

}