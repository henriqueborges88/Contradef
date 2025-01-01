
#include "DllFunctionMapper.h"

std::string GetFunctionFromAddress(unsigned long long address, unsigned int pid) {
    void* pAddress = reinterpret_cast<void*>(address);
    WindowsAPI::HANDLE process = WindowsAPI::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    char moduleName[MAX_PATH];
    std::string sModuleName;
    if (WindowsAPI::GetMappedFileNameA(process, pAddress, moduleName, MAX_PATH)) {
        sModuleName = std::string(moduleName);
    }
    else {
        return "Unknown Module";
    }

    WindowsAPI::HMODULE hModule = WindowsAPI::LoadLibraryA(getFileName(sModuleName).c_str());

    if (!hModule) {
        WindowsAPI::FreeLibrary(hModule);
        return "Invalid Module";
    }

    // Obter cabeçalho PE
    WindowsAPI::IMAGE_DOS_HEADER* dosHeader = (WindowsAPI::IMAGE_DOS_HEADER*)hModule;
    WindowsAPI::IMAGE_NT_HEADERS* ntHeaders = (WindowsAPI::IMAGE_NT_HEADERS*)((WindowsAPI::BYTE*)hModule + dosHeader->e_lfanew);

    DWORD exportDirRVA = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (exportDirRVA == 0) return "No export directory";

    WindowsAPI::IMAGE_EXPORT_DIRECTORY* exportDir = (WindowsAPI::IMAGE_EXPORT_DIRECTORY*)((WindowsAPI::BYTE*)hModule + exportDirRVA);
    DWORD* addressOfFunctions = (DWORD*)((WindowsAPI::BYTE*)hModule + exportDir->AddressOfFunctions);
    DWORD* addressOfNames = (DWORD*)((WindowsAPI::BYTE*)hModule + exportDir->AddressOfNames);
    WORD* addressOfNameOrdinals = (WORD*)((WindowsAPI::BYTE*)hModule + exportDir->AddressOfNameOrdinals);
    DWORD numberOfFunctions = exportDir->NumberOfFunctions;

    // Coletar e ordenar endereços de funções
    std::vector<std::pair<WindowsAPI::DWORD64, std::string>> functionAddresses;
    for (DWORD i = 0; i < numberOfFunctions; i++) {
        DWORD functionRVA = addressOfFunctions[i];
        WindowsAPI::DWORD64 functionAddress = (WindowsAPI::DWORD64)hModule + functionRVA;

        std::string functionName = "Unnamed Function";
        for (DWORD j = 0; j < exportDir->NumberOfNames; j++) {
            if (addressOfNameOrdinals[j] == i) {
                const char* name = (const char*)((WindowsAPI::BYTE*)hModule + addressOfNames[j]);
                functionName = name;
                break;
            }
        }

        functionAddresses.push_back(std::make_pair(functionAddress, functionName));
    }

    // Ordenar endereços
    std::sort(functionAddresses.begin(), functionAddresses.end(),
        [](const std::pair<WindowsAPI::DWORD64, std::string>& a, const std::pair<WindowsAPI::DWORD64, std::string>& b) {
            return a.first < b.first;
        });

    // Construir intervalos de funções
    std::vector<FunctionRange> functionRanges;
    for (size_t i = 0; i < functionAddresses.size(); i++) {
        FunctionRange range;
        range.start = functionAddresses[i].first;
        range.name = functionAddresses[i].second;
        range.end = (i + 1 < functionAddresses.size()) ? functionAddresses[i + 1].first : -1; // Última função até o final do espaço de endereços
        functionRanges.push_back(range);
    }

    // Procurar endereço no intervalo
    for (const auto& range : functionRanges) {
        if (address >= range.start && (range.end == -1 || address < range.end)) {
            WindowsAPI::FreeLibrary(hModule);
            return range.name.empty() ? "Unnamed Function" : range.name;
        }
    }

    WindowsAPI::FreeLibrary(hModule);
    return "Function not found";
}