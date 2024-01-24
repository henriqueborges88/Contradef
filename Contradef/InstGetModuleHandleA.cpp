/*
Para o Themida:
------> kernel32.dll
------> gdi32.dll
------> ole32.dll
------> oleaut32.dll
------> dateinj01.dll -> Pesquisar mais -> https://www.exedb.com/md/dateinj01---b67eb0f402c1ab9d6b5e9bcce61f80fb.shtml
------> cmdvrt32.dll -> Relacionado com COMODO Internet Security -> https://www.freefixer.com/library/file/cmdvrt32.dll-143267/  https://www.techtudo.com.br/tudo-sobre/comodo-internet-security/
------> SbieDll.dll -> Relacionado com sandboxie -> https://sandboxie-website-archive.github.io/www.sandboxie.com/index.html
------> KERNEL32.DLL
*/
#include "InstGetModuleHandleA.h"

std::map<CallContextKey, GetModuleHandleAArgs> InstGetModuleHandleA::argsMap;
ADDRINT InstGetModuleHandleA::nextCallId = 0;
std::ofstream* InstGetModuleHandleA::outStream;

VOID InstGetModuleHandleA::CallbackBefore(THREADID tid, ADDRINT callId, RTN* rtn, ADDRINT lpModuleName) {
    GetModuleHandleAArgs args;
    args.lpModuleName = lpModuleName;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleHandleA::CallbackAfter(THREADID tid, ADDRINT callId, RTN* rtn, ADDRINT* retValAddr, ADDRINT lpModuleName) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const GetModuleHandleAArgs& args = it->second;
        std::string ansiStringLpModuleName = ConvertAddrToAnsiString(args.lpModuleName);
        *outStream << std::endl << "[+] GetModuleHandleA..." << std::endl;
        *outStream << "    lpModuleName (parámetro): " << ansiStringLpModuleName << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}


VOID JIT_Instruction(CONTEXT* ctxt, INS ins, ADDRINT instAddress) {
    PIN_LockClient();

    std::cout << "[JIT] Instrução no endereço " << instAddress << std::endl;
    std::string disassembledInstruction = INS_Disassemble(ins);
    std::cout << "[JIT] Instrução " << disassembledInstruction << std::endl;



    IMG img = IMG_FindByAddress(instAddress);
    std::string moduleName = ExtractModuleName(IMG_Name(img));
    std::cout << "[JIT] moduleName " << moduleName << std::endl;

    
    
    // Obter o ponteiro para a pilha atual
    ADDRINT* stackPtr;
    PIN_GetContextRegval(ctxt, REG_STACK_PTR, reinterpret_cast<UINT8*>(&stackPtr));

    // Obter o endereço de retorno da pilha
    ADDRINT returnAddr = *stackPtr;

    // Encontrar a RTN que contém esse endereço
    RTN callerRTN = RTN_FindByAddress(returnAddr);
    std::string callerFcn = "";
    if (RTN_Valid(callerRTN)) {
        callerFcn = "Nome da rotina chamadora (possível): " + RTN_Name(callerRTN);
    }
    std::cout << "[JIT] " << callerFcn << std::endl;

    PIN_UnlockClient();

}

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    if (RTN_Name(rtn) == "GetModuleHandleA") {
        outStream = &out;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_ADDRINT, &nextCallId,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_ADDRINT, &nextCallId,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        // Instrumentar todas as instruções de uma rotina
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)JIT_Instruction,
                IARG_CONTEXT,
                IARG_PTR, ins,
                IARG_INST_PTR,
                IARG_END);
        }

        RTN_Close(rtn);
    }

}