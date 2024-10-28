// utils.cpp
#include "utils.h"

std::wstring ConvertAddrToWideString(ADDRINT addr) {
    if (addr != 0) {
        wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(addr);
        return std::wstring(wideCharStr);
    }
    else {
        return std::wstring();  // Retorna uma string vazia se o endereço for nulo
    }
}

std::string ConvertAddrToAnsiString(ADDRINT addr) {
    if (addr != 0) {
        const char* charStr = reinterpret_cast<const char*>(addr);
        return std::string(charStr);
    }
    else {
        return std::string();  // Retorna uma string vazia se o endereço for nulo
    }
}

std::wstring ReadWideString(ADDRINT addr) {
    std::wstring str;
    wchar_t ch;
    while (true) {
        // Copia um wchar_t do endereço do processo para 'ch'
        PIN_SafeCopy(&ch, reinterpret_cast<void*>(addr), sizeof(wchar_t));
        if (ch == L'\0') break;
        str += ch;
        addr += sizeof(wchar_t);
    }
    return str;
}


std::string WStringToString(const std::wstring& wstr) {
    std::string str(wstr.begin(), wstr.end());
    return str;
}

// Função para converter std::wstring para std::string
std::string ConvertWideStringToString(const std::wstring& wstr) {
    std::setlocale(LC_ALL, "en_US.UTF-8"); // Definir o local adequado

    std::mbstate_t state = std::mbstate_t();
    const wchar_t* src = wstr.c_str();
    size_t len = std::wcsrtombs(nullptr, &src, 0, &state) + 1;

    std::string dest(len, '\0');
    std::wcsrtombs(&dest[0], &src, len, &state);
    dest.resize(len - 1); // Remover o terminador nulo adicionado

    return dest;
}

std::string ExtractModuleName(const std::string& fullPath) {
    size_t lastSlashPos = fullPath.find_last_of("\\/");
    if (lastSlashPos != std::string::npos) {
        return fullPath.substr(lastSlashPos + 1);
    }
    else {
        return fullPath; // Retorna o fullPath original se não houver separador de diretório
    }
}

std::string toUpperCase(const std::string& str) {
    std::string upperCaseStr = str; // Cria uma cópia da string original
    for (char& c : upperCaseStr) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return upperCaseStr;
}

VOID PrintTrace(std::deque<std::string>& threadTraces) {
    std::cout << "Routine Trace:" << std::endl;
    for (const auto& name : threadTraces) {
        std::cout << "  " << name << std::endl;
    }
}


void PrintCallContextMap(std::map<CallContextKey, CallContext*>& callContextMap) {

    for (const auto& pair : callContextMap) {
        const CallContextKey& key = pair.first;
        const CallContext* context = pair.second;

        std::cout << "CallContextKey: (callCtxId: " << key.callId
            << ", tid: " << key.threadId << ")" << std::endl;

        std::cout << "  CallContext: " << std::endl;
        std::cout << "    callCtxId: " << context->callId << std::endl;
        std::cout << "    tid: " << context->threadId << std::endl;
        std::cout << "    rtnAddress: " << std::hex << context->rtnAddress << std::dec << std::endl;
        std::cout << "    stringStream: " << context->stringStream.str() << std::endl;
    }
}

// Função para converter std::string para const wchar_t*
const wchar_t* ConvertToWideString(const std::string& str)
{
    using namespace WindowsAPI;
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* wideStr = new wchar_t[size_needed];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideStr, size_needed);
    return wideStr;
}
