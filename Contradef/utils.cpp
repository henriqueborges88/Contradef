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

std::string CopyLPCSTR(ADDRINT addr) {
    const size_t MAX_STRING_LENGTH = 1024; // Defina o tamanho máximo da string
    char buffer[MAX_STRING_LENGTH];       // Buffer para armazenar a string copiada
    memset(buffer, 0, sizeof(buffer));    // Inicializa o buffer com zeros

    size_t copied = 0;                    // Contador de bytes copiados

    for (size_t i = 0; i < MAX_STRING_LENGTH - 1; i++) {
        // Use PIN_SafeCopy para copiar um único caractere por vez
        size_t result = PIN_SafeCopy(&buffer[i], reinterpret_cast<const void*>(addr + i), 1);

        // Se a cópia falhar ou encontrar o terminador nulo, finalize
        if (result == 0 || buffer[i] == '\0') {
            break;
        }

        copied++;
    }

    // Verifica se o tamanho máximo foi excedido
    if (copied == MAX_STRING_LENGTH - 1) {
        std::cerr << "Warning: String copy reached the maximum limit." << std::endl;
    }

    return std::string(buffer); // Retorna a string copiada como um std::string
}

void LogWString(const std::wstring& wstr) {
    std::string str = WStringToString(wstr);
    LOG("Converted String: %s\n", str.c_str());
}

std::string PrintNormalized(const std::wstring& wstr) {
    /*    
        Converter de addrint:    
        std::wstring wsBuffer = ConvertAddrToWideString(addr);
        std::cout << " STRING -> " << PrintNormalized(wsBuffer) << std::endl;
    */

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

std::string WideStringToStringFromPointer(ADDRINT addr)
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

void GetParentProcessName() {
    using namespace WindowsAPI;
    DWORD currentProcessId = GetCurrentProcessId();
    DWORD parentProcessId = 0;

    // Tirar um snapshot de todos os processos
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Erro ao criar snapshot.\n";
        return;
    }

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == currentProcessId) {
                parentProcessId = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (parentProcessId != 0) {
        // Tirar outro snapshot para procurar o processo pai
        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            std::cerr << "Erro ao criar snapshot.\n";
            return;
        }

        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == parentProcessId) {
                    std::wcout << L"Nome do processo pai: " << pe32.szExeFile << L"\n";
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
    }
    else {
        std::cerr << "Processo pai não encontrado.\n";
    }
}

DWORD GetProcessIdByName(std::string processName) {
    using namespace WindowsAPI;
    DWORD processId = 0;

    // Cria um snapshot de todos os processos
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Erro ao criar snapshot.\n";
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Percorre os processos no snapshot
    if (Process32First(hSnapshot, &pe32)) {
        do {
            // Compara o nome do executável
            std::transform(processName.begin(), processName.end(), processName.begin(), ::tolower);
            std::string pName = ConvertWideStringToString(pe32.szExeFile);
            std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
            if (processName == pName) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    else {
        std::cerr << "Erro ao percorrer os processos.\n";
    }

    CloseHandle(hSnapshot);
    return processId;
}