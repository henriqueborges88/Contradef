#include "InstNtOpenKey.h"

std::map<ADDRINT, InstNtOpenKeyArgs> InstNtOpenKey::argsMap;

VOID InstNtOpenKey::CallbackBefore(ADDRINT instrPtr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    InstNtOpenKeyArgs args;
    args.KeyHandle = KeyHandle;
    args.DesiredAccess = DesiredAccess;
    args.ObjectAttributes = ObjectAttributes;

    argsMap[instrPtr] = args;

    std::cout << "NtOpenKey será chamada com os seguintes parâmetros:" << std::endl;

    // Acessando a estrutura OBJECT_ATTRIBUTES
    OBJECT_ATTRIBUTES* objAttr = reinterpret_cast<OBJECT_ATTRIBUTES*>(ObjectAttributes);
    if (objAttr != nullptr && objAttr->ObjectName != nullptr) {
        UNICODE_STRING* unicodeStr = objAttr->ObjectName;
        std::wstring keyPath(unicodeStr->Buffer, unicodeStr->Length / sizeof(WCHAR));
        wprintf(L"[Contradef] NtOpenKey Caminho da chave: %ls\n", keyPath.c_str());
    }
}
//
//VOID InstNtOpenKey::CallbackBefore(ADDRINT instrPtr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
//    InstNtOpenKeyArgs args;
//    args.KeyHandle = KeyHandle;
//    args.DesiredAccess = DesiredAccess;
//    args.ObjectAttributes = ObjectAttributes;
//
//    argsMap[instrPtr] = args;
//
//    std::cout << "NtOpenKey foi chamada com os seguintes parâmetros:" << std::endl;
//    // Impressão dos parâmetros
//}

VOID InstNtOpenKey::CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    auto it = argsMap.find(instrPtr);
    if (it != argsMap.end()) {
        // Operações com args, se necessário
        argsMap.erase(it); // Limpar a entrada após o uso
    }

    std::cout << "Valor de retorno de NtOpenKey: " << *retValAddr << std::endl;
}

VOID InstNtOpenKey::InstrumentFunction(RTN rtn) {

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
