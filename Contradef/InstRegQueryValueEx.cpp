#include "InstRegQueryValueEx.h"

std::map<ADDRINT, RegQueryValueExArgs> InstRegQueryValueEx::argsMap;

VOID InstRegQueryValueEx::CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    RegQueryValueExArgs args;
    args.hKey = hKey;
    args.lpReserved = lpReserved;
    args.lpType = lpType;
    args.lpData = lpData;
    args.lpcbData = lpcbData;

    // Copiar a string apontada por lpValueName

    if (lpValueName != 0) {
        //wchar_t valueName[128];  // Ajuste o tamanho conforme necessário
        //size_t copiedBytes = PIN_SafeCopy(valueName, reinterpret_cast<void*>(lpValueName), (sizeof(valueName) - sizeof(wchar_t)));
        //if (copiedBytes > 0) {
        //    args.lpValueName = std::wstring(valueName);
        //}
        //cout << "copiado ->" << copiedBytes << endl;

        wchar_t* lpValueNameStr = reinterpret_cast<wchar_t*>(lpValueName);
        std::wstring wsLpValueName(lpValueNameStr);
        args.lpValueName = std::wstring(wsLpValueName.begin(), wsLpValueName.end());
        wprintf(L"[Contradef] RegQueryValue será chamada com lpValueName: %ls\n", args.lpValueName.c_str());
    }
    else {
        args.lpValueName.clear();  // Limpa a string se o ponteiro for NULL
        std::cout << "[Contradef] RegQueryValue será chamada com lpValueName: NULL" << std::endl;

    }

    argsMap[instrPtr] = args;

    //std::cout << "RegQueryValueExW será chamada com os seguintes parâmetros:" << std::endl;
    //std::cout << "HKEY: " << hKey << std::endl;
    //std::cout << "lpValueName (Endereço): " << lpValueName << std::endl;
    //std::cout << "lpReserved (Endereço): " << lpReserved << std::endl;
    //std::cout << "lpType (Endereço): " << lpType << std::endl;
    //std::cout << "lpData (Endereço): " << lpData << std::endl;
    //std::cout << "lpcbData (Endereço): " << lpcbData << std::endl;
   
}

VOID InstRegQueryValueEx::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpValueName, ADDRINT lpReserved, ADDRINT lpType, ADDRINT lpData, ADDRINT lpcbData) {
    //*retValAddr = 2;
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        const RegQueryValueExArgs& args = it->second;

        // Utilizar os valores armazenados
        if (!args.lpValueName.empty()) {
            wprintf(L"[Contradef] RegQueryValueEx foi chamada com lpValueName: %ls\n", args.lpValueName.c_str());
        }
        else {
            wprintf(L"[Contradef] RegQueryValueEx foi chamada com lpValueName: NULL\n");
        }

        if (args.lpData != NULL) {
            wprintf(L"[Contradef] RegQueryValueEx foi chamada com lpData: %ls\n", args.lpData);
        }
        else {
            wprintf(L"[Contradef] RegQueryValueEx foi chamada com lpData: NULL\n");
        }

        /*std::wstring wsMessage(reinterpret_cast<wchar_t*>(args.lpData));
        std::cout << "Valor:fffffffff- " << std::string(wsMessage.begin(), wsMessage.end()) << std::endl;*/

        // Limpeza após o uso
        argsMap.erase(it);
    }

    /*std::wstring wsMessage(reinterpret_cast<wchar_t*>(lpData));
    std::cout << "Valor:cccccccccc- " << std::string(wsMessage.begin(), wsMessage.end()) << std::endl;*/


}

VOID InstRegQueryValueEx::InstrumentFunction(RTN rtn) {
    PROTO protoRegQueryValueEx = PROTO_Allocate(PIN_PARG(ADDRINT), CALLINGSTD_STDCALL,
        "RegQueryValueExW",
        PIN_PARG(ADDRINT),   // HKEY é tratado como ADDRINT
        PIN_PARG(ADDRINT),   // LPCWSTR é tratado como ADDRINT (ponteiro)
        PIN_PARG(ADDRINT),   // LPDWORD é tratado como ADDRINT (ponteiro)
        PIN_PARG(ADDRINT),   // LPDWORD é tratado como ADDRINT (ponteiro)
        PIN_PARG(ADDRINT),   // LPBYTE é tratado como ADDRINT (ponteiro)
        PIN_PARG(ADDRINT),   // LPDWORD é tratado como ADDRINT (ponteiro)
        PIN_PARG_END());

    RTN_InsertCallProbed(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_PROTOTYPE, protoRegQueryValueEx,
        IARG_INST_PTR,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
        IARG_END);


    RTN_InsertCallProbed(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_PROTOTYPE, protoRegQueryValueEx,
        IARG_INST_PTR,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hKey
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // lpReserved
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lpType
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // lpData
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // lpcbData
        IARG_END);

    PROTO_Free(protoRegQueryValueEx);
}