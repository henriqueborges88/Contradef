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

VOID InstGetModuleHandleA::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpModuleName) {
    GetModuleHandleAArgs args;
    args.lpModuleName = lpModuleName;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;
}

VOID InstGetModuleHandleA::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpModuleName) {
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

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn, std::ofstream& out) {
    if (RTN_Name(rtn) == "GetModuleHandleA") {
        outStream = &out;

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_ADDRINT, &nextCallId,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_ADDRINT, &nextCallId,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_Close(rtn);
    }

}