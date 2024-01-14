#include "InstGetModuleFileNameW.h"

std::map<ADDRINT, InstGetModuleFileNameWArgs> InstGetModuleFileNameW::argsMap;

VOID InstGetModuleFileNameW::CallbackBefore(ADDRINT instrPtr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    InstGetModuleFileNameWArgs args;
    args.hModule = hModule;
    args.lpFilename = lpFilename;
    args.nSize = nSize;

    argsMap[instrPtr] = args;

}

VOID InstGetModuleFileNameW::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    wchar_t* lpFileNameStr = reinterpret_cast<wchar_t*>(lpFilename);
    std::wstring wsLpFileName(lpFileNameStr);
    std::wstring lpFilenameStr2 = std::wstring(wsLpFileName.begin(), wsLpFileName.end());
    wprintf(L"[Contradef] GetModuleFileNameW será chamada com lpFilename: %ls\n", lpFilenameStr2.c_str());

    std::cout << "Valor de retorno de GetModuleFileNameW: " << *retValAddr << std::endl;
}

VOID InstGetModuleFileNameW::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
        IARG_END);

    RTN_Close(rtn);

}
