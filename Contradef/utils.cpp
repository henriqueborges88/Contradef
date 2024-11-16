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
    std::wstring result;
    wchar_t currentChar;
    ADDRINT currentAddr = addr; // Usar uma cópia do endereço para evitar alterar o original

    // Lê caractere por caractere a partir do endereço fornecido até encontrar o terminador nulo
    while (PIN_SafeCopy(&currentChar, reinterpret_cast<wchar_t*>(currentAddr), sizeof(wchar_t)) == sizeof(wchar_t) && currentChar != L'\0') {
        result += currentChar;
        currentAddr += sizeof(wchar_t); // Incrementa o endereço para o próximo wchar_t
    }

    return result;
}

std::string WStringToString(const std::wstring& wstr) {
    std::string str;
    for (wchar_t wc : wstr) {
        if (wc < 128) {
            str += static_cast<char>(wc);
        }
        else {
            // Converte caracteres fora do ASCII
            char buffer[5] = { 0 };
            if (wc < 0x0800) {
                buffer[0] = 0xC0 | ((wc >> 6) & 0x1F);
                buffer[1] = 0x80 | (wc & 0x3F);
            }
            else {
                buffer[0] = 0xE0 | ((wc >> 12) & 0x0F);
                buffer[1] = 0x80 | ((wc >> 6) & 0x3F);
                buffer[2] = 0x80 | (wc & 0x3F);
            }
            str += buffer;
        }
    }
    return str;
}

void LogWString(const std::wstring& wstr) {
    std::string str = WStringToString(wstr);
    LOG("Converted String: %s\n", str.c_str());
}

std::string PrintNormalized(const std::wstring& wstr) {
    std::string str;
    for (wchar_t wc : wstr) {
        if (wc < 128) {
            str += static_cast<char>(wc);  // Simplificação: assume que o caractere está dentro do ASCII
        }
        else {
            str += '?'; // Caracteres fora do ASCII são substituídos por '?'
        }
    }
    return str.c_str();
}

//
//std::string WStringToString(const std::wstring& wstr) {
//    std::string str(wstr.begin(), wstr.end());
//    return str;
//}

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


std::wstring ReadWideStringFromAddrint(ADDRINT addr)
{
    using namespace WindowsAPI;
    wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(addr);
    if (wideCharStr == nullptr)
    {
        return L"";
    }

    // Calcula o comprimento da string apontada
    size_t length = wcslen(wideCharStr);

    // Constrói uma wstring a partir do ponteiro
    return std::wstring(wideCharStr, length);
}



std::string ConvertWStringToUTF8(const std::wstring& wstr)
{
    std::string result;
    for (wchar_t wc : wstr)
    {
        if (wc <= 0x7F) // ASCII character
        {
            result.push_back(static_cast<char>(wc));
        }
        else if (wc <= 0x7FF) // 2-byte UTF-8
        {
            result.push_back(static_cast<char>(0xC0 | ((wc >> 6) & 0x1F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        }
        else // 3-byte UTF-8
        {
            result.push_back(static_cast<char>(0xE0 | ((wc >> 12) & 0x0F)));
            result.push_back(static_cast<char>(0x80 | ((wc >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (wc & 0x3F)));
        }
    }
    return result;
}

std::string PrintWideStringFromPointer(ADDRINT addr)
{
    wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(addr);
    if (wideCharStr == nullptr)
    {
        return std::string();  // Retorna uma string vazia se o endereço for nulo
    }

    std::wstring wstr(wideCharStr);
    std::string str = ConvertWStringToUTF8(wstr);
    return str;
}