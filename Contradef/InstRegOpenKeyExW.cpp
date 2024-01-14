#include "InstRegOpenKeyExW.h"

std::map<ADDRINT, RegOpenKeyExWArgs> InstRegOpenKeyExW::argsMap;

VOID InstRegOpenKeyExW::CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    RegOpenKeyExWArgs args;
    args.hKey = hKey;
    args.ulOptions = ulOptions;
    args.samDesired = samDesired;
    args.phkResult = phkResult;

    if (lpSubKey != 0) {
        wchar_t* lpSubKeyStr = reinterpret_cast<wchar_t*>(lpSubKey);
        std::wstring wsLpSubKey(lpSubKeyStr);
        args.lpSubKey = std::wstring(wsLpSubKey.begin(), wsLpSubKey.end());
    }
    else {
        args.lpSubKey.clear();  // Limpa a string se o ponteiro for NULL
    }

    argsMap[instrPtr] = args;

    wprintf(L"[Contradef] RegOpenKeyExW será chamada com lpSubKey: %ls\n", args.lpSubKey.c_str());
    /*std::cout << "RegOpenKeyExW será chamada com os seguintes parâmetros:" << std::endl;
    std::cout << "HKEY: " << hKey << std::endl;
    std::cout << "ulOptions: " << ulOptions << std::endl;
    std::cout << "samDesired: " << samDesired << std::endl;
    std::cout << "phkResult (Endereço): " << phkResult << std::endl;*/

}

VOID InstRegOpenKeyExW::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const RegOpenKeyExWArgs& args = it->second;

        // Utilizar os valores armazenados
        wprintf(L"[Contradef] RegOpenKeyExW foi chamada com lpSubKey: %ls\n", args.lpSubKey.c_str());

        // Limpeza após o uso
        argsMap.erase(it);
    }

    std::cout << "[Contradef] Valor de retorno de RegOpenKeyExW: " << *retValAddr << std::endl;
}

VOID InstRegOpenKeyExW::InstrumentFunction(RTN rtn) {

    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpSubKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ulOptions
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // samDesired
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // phkResult
        IARG_END);

    RTN_Close(rtn);

}

