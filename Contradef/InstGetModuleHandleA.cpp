#include "InstGetModuleHandleA.h"

std::map<ADDRINT, GetModuleHandleAArgs> InstGetModuleHandleA::argsMap;

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

// Esta versão com PIN_SafeCopy crasha a aplicação analisada
//VOID CallbackBeforeGetModuleHandleA(ADDRINT lpModuleName)
//{
//    // Verificar se o ponteiro não é NULL
//    if (lpModuleName != 0) {
//        // Converter de ADDRINT para LPCSTR
//        const char* moduleNameStr = reinterpret_cast<const char*>(lpModuleName);
//
//        // Acessar a string com segurança
//        char moduleNameBuffer[256] = { 0 };
//        PIN_SafeCopy(moduleNameBuffer, moduleNameStr, sizeof(moduleNameBuffer));
//
//        std::cout << "GetModuleHandleA será chamada com lpModuleName: " << moduleNameBuffer << std::endl;
//    }
//    else {
//        std::cout << "GetModuleHandleA será chamada com lpModuleName: NULL" << std::endl;
//    }
//}

VOID InstGetModuleHandleA::CallbackBefore(ADDRINT instrPtr, ADDRINT lpModuleName) {
    GetModuleHandleAArgs args;
    args.lpModuleName = lpModuleName;

    argsMap[instrPtr] = args;
    const char* moduleNameStr = reinterpret_cast<const char*>(lpModuleName);
    std::cout << "[Contradef] GetModuleHandleA invocada com lpModuleName: " << moduleNameStr << std::endl;
}

VOID InstGetModuleHandleA::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpModuleName) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const GetModuleHandleAArgs& args = it->second;
        const char* moduleNameStr = reinterpret_cast<const char*>(args.lpModuleName);
       // Usar moduleNameStr
    }

    std::cout << "[Contradef] Valor de retorno de GetModuleHandleA: " << *retValAddr << std::endl;
}

VOID InstGetModuleHandleA::InstrumentFunction(RTN rtn) {
    if (RTN_Name(rtn) == "GetModuleHandleA") {
        std::cout << "Instrumentando GetModuleHandleA" << std::endl;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_INST_PTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_INST_PTR,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpModuleName
            IARG_END);

        RTN_Close(rtn);
    }
}
