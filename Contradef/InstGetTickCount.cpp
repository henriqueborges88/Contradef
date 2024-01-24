#include "InstGetTickCount.h"

/*
A função GetTickCount não possui argumentos de entrada. Ela retorna o número de milissegundos desde que o sistema foi iniciado. Portanto, a estrutura InstGetTickCountArgs é mantida vazia.
*/

std::ofstream* InstGetTickCount::outStream;

VOID InstGetTickCount::CallbackBefore(ADDRINT instrPtr) {
    //std::cout << "GetTickCount será chamada." << std::endl;
}

VOID InstGetTickCount::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr) {
    *outStream << std::endl << "[+] GetTickCount..." << std::endl;
    *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
    *outStream << "[*] Concluído" << std::endl << std::endl;
}

VOID InstGetTickCount::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_END);

    RTN_Close(rtn);

}
