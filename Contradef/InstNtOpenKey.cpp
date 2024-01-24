#include "InstNtOpenKey.h"

std::map<CallContextKey, InstNtOpenKeyArgs> InstNtOpenKey::argsMap;
ADDRINT InstNtOpenKey::nextCallId = 0;
std::ofstream* InstNtOpenKey::outStream;

VOID InstNtOpenKey::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    InstNtOpenKeyArgs args;
    args.KeyHandle = KeyHandle;
    args.DesiredAccess = DesiredAccess;
    args.ObjectAttributes = ObjectAttributes;

    // Acessando a estrutura OBJECT_ATTRIBUTES
    OBJECT_ATTRIBUTES* objAttr = reinterpret_cast<OBJECT_ATTRIBUTES*>(ObjectAttributes);
    if (objAttr != nullptr && objAttr->ObjectName != nullptr) {
        UNICODE_STRING* unicodeStr = objAttr->ObjectName;
        std::wstring keyPath(unicodeStr->Buffer, unicodeStr->Length / sizeof(WCHAR));
        args.keyPathStr = WStringToString(keyPath);
    }

    CallContextKey key = { callId, tid };
    argsMap[key] = args;


}

VOID InstNtOpenKey::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        //// Acessando a estrutura OBJECT_ATTRIBUTES
        //// Erro ao tentar acessar depois da execução, possivelmente o espaço em memoria 
        //// da estrutura OBJECT_ATTRIBUTES foi sobrescrita durante a execução, se necessário, 
        //// considerar copiar o valor do texto para outro espaço em memoria antes da execução, 
        //// logo resgatar esse valor copiado no lugar da estrutura original
        const InstNtOpenKeyArgs& args = it->second;
        *outStream << std::endl << "[+] NtOpenKey..." << std::endl;
        *outStream << "    Caminho da chave: " << args.keyPathStr << std::endl;
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstNtOpenKey::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // DesiredAccess
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ObjectAttributes
        IARG_END);
    RTN_Close(rtn);

}