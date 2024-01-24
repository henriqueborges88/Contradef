#include "InstNtQueryValueKey.h"
#include "utils.h"
#include "NtStructures.h"

#ifndef REG_NONE
#define REG_NONE 0
#endif

#ifndef REG_SZ
#define REG_SZ 1
#endif

#ifndef REG_EXPAND_SZ
#define REG_EXPAND_SZ 2
#endif

#ifndef REG_BINARY
#define REG_BINARY 3
#endif

#ifndef REG_DWORD
#define REG_DWORD 4
#endif

#ifndef REG_MULTI_SZ
#define REG_MULTI_SZ 7
#endif

#ifndef REG_QWORD
#define REG_QWORD 11
#endif

std::map<CallContextKey, InstNtQueryValueKeyArgs> InstNtQueryValueKey::argsMap;
ADDRINT InstNtQueryValueKey::nextCallId = 0;
std::ofstream* InstNtQueryValueKey::outStream;

VOID InstNtQueryValueKey::InterpretKeyValueInformationFull(BYTE* keyValueInformation) {
    //KEY_VALUE_PARTIAL_INFORMATION* info;
    //info = (KEY_VALUE_PARTIAL_INFORMATION*)keyValueInformation;

    auto info = reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(keyValueInformation);

    *outStream << "    Tipo do valor: " << info->Type << std::endl;
    *outStream << "    Tamanho do dado: " << std::dec << info->DataLength << " bytes" << std::endl;

    BYTE* data = keyValueInformation + info->DataOffset;

    switch (info->Type) {
    case REG_NONE: {
        *outStream << "    Valor (REG_NONE): ";
        if (info->DataLength > 100000) {
            *outStream << "    Tamanho excede o limite";
        }
        else {
            for (ULONG i = 0; i < info->DataLength; ++i) {
                *outStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]) << " ";
            }
        }
        *outStream << "     (dados binários brutos)" << std::endl;
        break;
    }
    case REG_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        *outStream << "    Valor (string): " << WStringToString(stringValue) << std::endl;
        break;
    }
    case REG_EXPAND_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        *outStream << "    Valor (string): " << WStringToString(stringValue) << std::endl;
        break;
    }
    case REG_DWORD: {
        if (info->DataLength >= sizeof(DWORD)) {
            DWORD dwordValue = *reinterpret_cast<DWORD*>(data);
            *outStream << "    Valor (DWORD): " << dwordValue << std::endl;
        }
        break;
    }
    case REG_BINARY: {
        *outStream << "    Valor (binário): ";
        for (ULONG i = 0; i < info->DataLength; ++i) {
            *outStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]) << " ";
        }
        std::cout << std::endl;
        break;
    }
    case REG_MULTI_SZ: {
        const WCHAR* multiSz = reinterpret_cast<const WCHAR*>(data);
        std::cout << L"Valor (multi-string): ";

        while (*multiSz) {
            std::wstring str(multiSz);
            *outStream << "    " << WStringToString(str) << " | ";
            multiSz += str.size() + 1;
        }
        *outStream << std::endl;
        break;
    }
    case REG_QWORD: {
        if (info->DataLength >= sizeof(ULONGLONG)) {
            ULONGLONG qwordValue = *reinterpret_cast<ULONGLONG*>(data);
            *outStream << "    Valor (QWORD): " << qwordValue << std::endl;
        }
        break;
    }
    default:
        *outStream << "    Tipo de dado não reconhecido ou não suportado." << std::endl;
    }
}

VOID InstNtQueryValueKey::InterpretKeyValueInformationPartial(BYTE* keyValueInformation) {
    auto info = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(keyValueInformation);

    *outStream << "    Tipo do valor: " << info->Type << std::endl;
    *outStream << "    Tamanho do dado: " << std::dec << info->DataLength << " bytes" << std::endl;

    // O dado é armazenado diretamente na estrutura após os campos iniciais
    BYTE* data = info->Data;

    // Exemplo de interpretação de dados, dependendo do tipo
    switch (info->Type) {
    case REG_NONE: {
        *outStream << "    Valor (REG_NONE): ";
        if (info->DataLength > 100000) {
            *outStream << "    Tamanho excede o limite";
        }
        else {
            for (ULONG i = 0; i < info->DataLength; ++i) {
                *outStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]) << " ";
            }
        }
        *outStream << "     (dados binários brutos)" << std::endl;
        break;
    }
    case REG_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        *outStream << "    Valor (string): " << WStringToString(stringValue) << std::endl;
        break;
    }
    case REG_EXPAND_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        *outStream << "    Valor (string): " << WStringToString(stringValue) << std::endl;
        break;
    }
    case REG_DWORD: {
        if (info->DataLength >= sizeof(DWORD)) {
            DWORD dwordValue = *reinterpret_cast<DWORD*>(data);
            *outStream << "    Valor (DWORD): " << dwordValue << std::endl;
        }
        break;
    }
    case REG_BINARY: {
        *outStream << "    Valor (binário): ";
        for (ULONG i = 0; i < info->DataLength; ++i) {
            *outStream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]) << " ";
        }
        std::cout << std::endl;
        break;
    }
    case REG_MULTI_SZ: {
        const WCHAR* multiSz = reinterpret_cast<const WCHAR*>(data);
        std::cout << L"Valor (multi-string): ";

        while (*multiSz) {
            std::wstring str(multiSz);
            *outStream << "    " << WStringToString(str) << " | ";
            multiSz += str.size() + 1;
        }
        *outStream << std::endl;
        break;
    }
    case REG_QWORD: {
        if (info->DataLength >= sizeof(ULONGLONG)) {
            ULONGLONG qwordValue = *reinterpret_cast<ULONGLONG*>(data);
            *outStream << "    Valor (QWORD): " << qwordValue << std::endl;
        }
        break;
    }
    default:
        *outStream << "    Tipo de dado não reconhecido ou não suportado." << std::endl;
    }
}


VOID InstNtQueryValueKey::InterpretKeyValueInformationBasic(BYTE* keyValueInformation) {
    auto info = reinterpret_cast<KEY_VALUE_BASIC_INFORMATION*>(keyValueInformation);

    *outStream << "    Tipo do valor: " << info->Type << std::endl;
    *outStream << "    Comprimento do nome do valor: " << std::dec << info->NameLength << " bytes" << std::endl;

    // O nome do valor é armazenado diretamente na estrutura após os campos iniciais
    std::wstring valueName(info->Name, info->NameLength / sizeof(WCHAR));
    *outStream << "    Nome do valor: " << WStringToString(valueName) << std::endl;

}

VOID InstNtQueryValueKey::CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT KeyHandle, ADDRINT ValueName, USHORT KeyValueInformationClass, ADDRINT KeyValueInformation, UINT Length, UINT ResultLength) {
    InstNtQueryValueKeyArgs args;
    args.KeyHandle = KeyHandle;
    args.ValueName = ValueName;
    args.KeyValueInformationClass = KeyValueInformationClass;
    args.KeyValueInformation = KeyValueInformation;
    args.Length = Length;
    args.ResultLength = ResultLength;

    CallContextKey key = { callId, tid };
    argsMap[key] = args;

}

VOID InstNtQueryValueKey::CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT ValueName, USHORT KeyValueInformationClass, ADDRINT KeyValueInformation, UINT Length, UINT ResultLength) {
    CallContextKey key = { callId, tid };
    auto it = argsMap.find(key);
    if (it != argsMap.end()) {
        const InstNtQueryValueKeyArgs& args = it->second;
        *outStream << std::endl << "[+] NtQueryValueKey..." << std::endl;
        

        UNICODE_STRING* unicodeString = reinterpret_cast<UNICODE_STRING*>(args.ValueName);
        if (unicodeString && unicodeString->Buffer) {
            WCHAR* wideCharStr = reinterpret_cast<WCHAR*>(unicodeString->Buffer);
            std::wstring valueNameStr = std::wstring(wideCharStr);
            *outStream << "    ValueName (parámetro): " << WStringToString(valueNameStr) << std::endl;
        }


        BYTE* KeyValueInformationBuffer = reinterpret_cast<BYTE*>(args.KeyValueInformation);
        if (args.KeyValueInformationClass == 0) {
            InterpretKeyValueInformationBasic(KeyValueInformationBuffer);
        }
        else if(args.KeyValueInformationClass == 1) {
            InterpretKeyValueInformationFull(KeyValueInformationBuffer);
        }
        else if (args.KeyValueInformationClass == 2) {
            InterpretKeyValueInformationPartial(KeyValueInformationBuffer);
        }
        *outStream << "    Valor de retorno: " << *retValAddr << std::endl;
        *outStream << "[*] Concluído" << std::endl << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstNtQueryValueKey::InstrumentFunction(RTN rtn, std::ofstream& out) {
    outStream = &out;
    
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // KeyValueInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // KeyValueInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // Length
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // ResultLength
        IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
        IARG_THREAD_ID,
        IARG_ADDRINT, &nextCallId,
        IARG_REG_REFERENCE, REG_GAX,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // KeyHandle
        IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ValueName
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // KeyValueInformationClass
        IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // KeyValueInformation
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // Length
        IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // ResultLength
        IARG_END);

    RTN_Close(rtn);

}