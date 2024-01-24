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