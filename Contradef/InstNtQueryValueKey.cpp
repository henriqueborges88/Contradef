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

VOID InstNtQueryValueKey::InterpretKeyValueInformationFull(BYTE* keyValueInformation) {
    //KEY_VALUE_PARTIAL_INFORMATION* info;
    //info = (KEY_VALUE_PARTIAL_INFORMATION*)keyValueInformation;

    auto info = reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(keyValueInformation);

    std::cout << "Tipo do valor: " << info->Type << std::endl;
    std::cout << "Tamanho do dado: " << info->DataLength << " bytes" << std::endl;

    BYTE* data = keyValueInformation + info->DataOffset;

    switch (info->Type) {
    case REG_NONE: {
        std::cout << "Valor (REG_NONE): ";
        if (info->DataLength > 100000) {
            std::cout << "Tamanho excede o limite";
        }
        else {
            for (ULONG i = 0; i < info->DataLength; ++i) {
                printf("%02X ", data[i]);
            }
        }
        std::cout << " (dados binários brutos)" << std::endl;
        break;
    }
    case REG_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        wprintf(L"Valor (string): %ls\n", stringValue.c_str());
        break;
    }
    case REG_EXPAND_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        wprintf(L"Valor (string): %ls\n", stringValue.c_str());
        break;
    }
    case REG_DWORD: {
        if (info->DataLength >= sizeof(DWORD)) {
            DWORD dwordValue = *reinterpret_cast<DWORD*>(data);
            std::cout << "Valor (DWORD): " << dwordValue << std::endl;
        }
        break;
    }
    case REG_BINARY: {
        std::cout << "Valor (binário): ";
        for (ULONG i = 0; i < info->DataLength; ++i) {
            printf("%02X ", data[i]);
        }
        std::cout << std::endl;
        break;
    }
    case REG_MULTI_SZ: {
        const WCHAR* multiSz = reinterpret_cast<const WCHAR*>(data);
        std::cout << L"Valor (multi-string): ";

        while (*multiSz) {
            std::wstring str(multiSz);
            wprintf(L"%ls | ", str.c_str());
            multiSz += str.size() + 1;
        }
        wprintf(L"\n");
        break;
    }
    case REG_QWORD: {
        if (info->DataLength >= sizeof(ULONGLONG)) {
            ULONGLONG qwordValue = *reinterpret_cast<ULONGLONG*>(data);
            std::cout << "Valor (QWORD): " << qwordValue << std::endl;
        }
        break;
    }
    default:
        std::cout << "Tipo de dado não reconhecido ou não suportado." << std::endl;
    }
}

VOID InstNtQueryValueKey::InterpretKeyValueInformationPartial(BYTE* keyValueInformation) {
    auto info = reinterpret_cast<KEY_VALUE_PARTIAL_INFORMATION*>(keyValueInformation);

    std::cout << "Tipo do valor: " << info->Type << std::endl;
    std::cout << "Tamanho do dado: " << info->DataLength << " bytes" << std::endl;

    // O dado é armazenado diretamente na estrutura após os campos iniciais
    BYTE* data = info->Data;

    // Exemplo de interpretação de dados, dependendo do tipo
    switch (info->Type) {
    case REG_NONE: {
        std::cout << "Valor (REG_NONE): ";
        if (info->DataLength > 100000) {
            std::cout << "Tamanho excede o limite";
        }
        else {
            for (ULONG i = 0; i < info->DataLength; ++i) {
                printf("%02X ", data[i]);
            }
        }
        std::cout << " (dados binários brutos)" << std::endl;
        break;
    }
    case REG_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        wprintf(L"Valor (string): %ls\n", stringValue.c_str());
        break;
    }
    case REG_EXPAND_SZ: {
        std::wstring stringValue(reinterpret_cast<WCHAR*>(data), info->DataLength / sizeof(WCHAR));
        wprintf(L"Valor (string): %ls\n", stringValue.c_str());
        break;
    }
    case REG_DWORD: {
        if (info->DataLength >= sizeof(DWORD)) {
            DWORD dwordValue = *reinterpret_cast<DWORD*>(data);
            std::cout << "Valor (DWORD): " << dwordValue << std::endl;
        }
        break;
    }
    case REG_BINARY: {
        std::cout << "Valor (binário): ";
        for (ULONG i = 0; i < info->DataLength; ++i) {
            printf("%02X ", data[i]);
        }
        std::cout << std::endl;
        break;
    }
    case REG_MULTI_SZ: {
        const WCHAR* multiSz = reinterpret_cast<const WCHAR*>(data);
        std::cout << L"Valor (multi-string): ";

        while (*multiSz) {
            std::wstring str(multiSz);
            wprintf(L"%ls | ", str.c_str());
            multiSz += str.size() + 1;
        }
        wprintf(L"\n");
        break;
    }
    case REG_QWORD: {
        if (info->DataLength >= sizeof(ULONGLONG)) {
            ULONGLONG qwordValue = *reinterpret_cast<ULONGLONG*>(data);
            std::cout << "Valor (QWORD): " << qwordValue << std::endl;
        }
        break;
    }
    default:
        std::cout << "Tipo de dado não reconhecido ou não suportado." << std::endl;
    }
}


VOID InstNtQueryValueKey::InterpretKeyValueInformationBasic(BYTE* keyValueInformation) {
    auto info = reinterpret_cast<KEY_VALUE_BASIC_INFORMATION*>(keyValueInformation);

    std::cout << "Tipo do valor: " << info->Type << std::endl;
    std::cout << "Comprimento do nome do valor: " << info->NameLength << " bytes" << std::endl;

    // O nome do valor é armazenado diretamente na estrutura após os campos iniciais
    std::wstring valueName(info->Name, info->NameLength / sizeof(WCHAR));
    wprintf(L"Nome do valor: %ls\n", valueName.c_str());
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

        UNICODE_STRING* unicodeString = reinterpret_cast<UNICODE_STRING*>(args.ValueName);
        if (unicodeString && unicodeString->Buffer) {
            WCHAR* wideCharStr = reinterpret_cast<WCHAR*>(unicodeString->Buffer);
            std::wstring valueNameStr = std::wstring(wideCharStr);
            wprintf(L"[Contradef] NtQueryValueKey ValueName: %ls\n", valueNameStr.c_str());
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


        std::cout << "[Contradef] NtQueryValueKey completou a execução, valor de retorno: " << *retValAddr << std::endl;
        argsMap.erase(it); // Limpar a entrada após o uso
    }
}

VOID InstNtQueryValueKey::InstrumentFunction(RTN rtn) {
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