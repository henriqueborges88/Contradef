#include "pin.H"
#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include "InstRegQueryValueEx.h"
#include "InstRegOpenKeyEx.h"
#include "InstNtQueryInformationProcess.h"
#include "InstGetModuleHandleA.h"
#include "InstGetModuleHandleW.h"
#include "InstIsDebuggerPresent.h"
#include "InstCheckRemoteDebuggerPresent.h"
#include "InstOutputDebugString.h"
#include "InstQueryPerformanceCounter.h"
#include "InstBlockInput.h"
#include "InstGetAsyncKeyState.h"
#include "InstGetForegroundWindow.h"
#include "InstGetTickCount.h"
#include "InstNtQuerySystemInformation.h"
#include "InstGetModuleFileNameA.h"

//extern "C" __declspec(dllimport) void __stdcall Sleep(unsigned long dwMilliseconds);

using std::string;
using std::cerr;
using std::cout;
using std::wcout;
using std::wcerr;
using std::endl;
using std::flush;
using std::set;
using std::wstring;

// API Types
//typedef UINT64 HWND;   // HWND é normalmente um ponteiro ou um valor integral
//typedef const wchar_t* LPCWSTR;  // Para LPCWSTR, use um ponteiro para wchar_t
//typedef unsigned int UINT;   // UINT é um inteiro sem sinal


std::ofstream TraceFile;
set<string> functionsToIntercept;  // Lista de funções a serem interceptadas

VOID FillInterceptSet() {
    functionsToIntercept.insert("USER32.dll:MessageBoxW");
    functionsToIntercept.insert("ADVAPI32.DLL:RegOpenKeyExW");
    functionsToIntercept.insert("ADVAPI32.DLL:RegQueryValueExW");
    functionsToIntercept.insert("KERNEL32.DLL:GetModuleHandleA");
    functionsToIntercept.insert("KERNEL32.DLL:GetModuleHandleW");
    functionsToIntercept.insert("KERNEL32.DLL:GetModuleFileNameA"); 
    functionsToIntercept.insert("KERNEL32.DLL:IsDebuggerPresent");
    functionsToIntercept.insert("KERNEL32.DLL:CheckRemoteDebuggerPresent");
    functionsToIntercept.insert("KERNEL32.DLL:OutputDebugString");
    functionsToIntercept.insert("KERNEL32.DLL:QueryPerformanceCounter");
    functionsToIntercept.insert("NTDLL.DLL:NtQueryInformationProcess");
    functionsToIntercept.insert("NTDLL.DLL:NtQuerySystemInformation");
    functionsToIntercept.insert("NTDLL.DLL:NtGlobalFlag");
    functionsToIntercept.insert("USER32.dll:BlockInput");
    functionsToIntercept.insert("USER32.dll:GetAsyncKeyState");
    functionsToIntercept.insert("USER32.dll:GetForegroundWindow");
    functionsToIntercept.insert("KERNEL32.DLL:GetTickCount");
    /*
    * Incluir InstRegQueryValue se o Ex, pode ter o InstRegQueryValueA e o InstRegQueryValueW
    * 
    EnumProcessModules/EnumProcessModulesEx: Essas funções podem ser usadas para enumerar os módulos carregados em um processo. Isso é útil para detectar DLLs suspeitas ou inesperadas que podem ser injetadas por instrumentadores binários.

    GetModuleHandle/GetModuleFileName: Essas funções ajudam a obter handles e nomes de arquivos de módulos específicos. Você pode usá-las para verificar se um módulo conhecido de um instrumentador DBI, como o dynamorio.dll (usado pelo DynamoRIO) ou pinvm.dll (usado pelo Intel PIN), está carregado em um processo.

    VirtualQuery/VirtualProtect: Estas funções permitem inspecionar e modificar as permissões de páginas de memória. Instrumentadores DBI frequentemente modificam permissões de memória para injetar código ou realizar a instrumentação.

    CreateToolhelp32Snapshot/Module32First/Module32Next: Estas funções são usadas para tirar um 'snapshot' dos processos e módulos em execução no sistema. Eles podem ser usados para detectar alterações suspeitas no espaço de endereço de um processo.

    Debug API (como DebugActiveProcess): A API de Debug do Windows pode ser utilizada para anexar a um processo e monitorar suas atividades, o que pode revelar a presença de DBIs.

    Event Tracing for Windows (ETW): O ETW fornece um mecanismo de rastreamento de eventos de alto desempenho. Você pode usar o ETW para monitorar eventos do sistema e de aplicativos que podem indicar a presença de instrumentação binária.

    NtQueryInformationProcess: Esta função de baixo nível pode ser usada para obter informações detalhadas sobre um processo, incluindo a presença de hooks ou alterações suspeitas no espaço de endereço.

    System Service Descriptor Table (SSDT) Hooking: Verificar alterações no SSDT, pois os DBIs podem modificar esta tabela para interceptar chamadas de sistema.
    */
}

std::string toUpperCase(const std::string& str) {
    std::string upperCaseStr = str; // Cria uma cópia da string original
    for (char& c : upperCaseStr) {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return upperCaseStr;
}

std::set<std::string> GetFunctionsOfModule(const std::string& moduleName) {
    std::set<std::string> moduleFunctions;
    for (const auto& func : functionsToIntercept) {
        if (func.find(moduleName + ":") == 0) {
            moduleFunctions.insert(func.substr(moduleName.size() + 1));
        }
    }
    return moduleFunctions;
}

string ExtractModuleName(const std::string& fullPath) {
    size_t lastSlashPos = fullPath.find_last_of("\\/");
    if (lastSlashPos != std::string::npos) {
        return fullPath.substr(lastSlashPos + 1);
    }
    else {
        return fullPath; // Retorna o fullPath original se não houver separador de diretório
    }
}

/*
Para detectar Instrumentadores Binários Dinâmicos (DBI, na sigla em inglês) em sistemas operacionais Windows, você pode utilizar várias funções da API do Windows. Essas funções permitem monitorar e analisar a atividade do sistema para identificar possíveis sinais de instrumentação binária. Algumas das funções e técnicas mais relevantes incluem:

    EnumProcessModules/EnumProcessModulesEx: Essas funções podem ser usadas para enumerar os módulos carregados em um processo. Isso é útil para detectar DLLs suspeitas ou inesperadas que podem ser injetadas por instrumentadores binários.

    GetModuleHandle/GetModuleFileName: Essas funções ajudam a obter handles e nomes de arquivos de módulos específicos. Você pode usá-las para verificar se um módulo conhecido de um instrumentador DBI, como o dynamorio.dll (usado pelo DynamoRIO) ou pinvm.dll (usado pelo Intel PIN), está carregado em um processo.

    VirtualQuery/VirtualProtect: Estas funções permitem inspecionar e modificar as permissões de páginas de memória. Instrumentadores DBI frequentemente modificam permissões de memória para injetar código ou realizar a instrumentação.

    CreateToolhelp32Snapshot/Module32First/Module32Next: Estas funções são usadas para tirar um 'snapshot' dos processos e módulos em execução no sistema. Eles podem ser usados para detectar alterações suspeitas no espaço de endereço de um processo.

    Debug API (como DebugActiveProcess): A API de Debug do Windows pode ser utilizada para anexar a um processo e monitorar suas atividades, o que pode revelar a presença de DBIs.

    Event Tracing for Windows (ETW): O ETW fornece um mecanismo de rastreamento de eventos de alto desempenho. Você pode usar o ETW para monitorar eventos do sistema e de aplicativos que podem indicar a presença de instrumentação binária.

    NtQueryInformationProcess: Esta função de baixo nível pode ser usada para obter informações detalhadas sobre um processo, incluindo a presença de hooks ou alterações suspeitas no espaço de endereço.

    System Service Descriptor Table (SSDT) Hooking: Verificar alterações no SSDT, pois os DBIs podem modificar esta tabela para interceptar chamadas de sistema.

Lembre-se de que a eficácia dessas técnicas pode variar dependendo do tipo específico de instrumentador binário e das técnicas de evasão que ele emprega. Além disso, essas funções requerem um entendimento profundo do funcionamento interno do Windows e do desenvolvimento de software de baixo nível.
*/

VOID InstrumentFunctions(IMG img, VOID* v) {
    string moduleName = ExtractModuleName(IMG_Name(img)); ;
    for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {
        RTN rtn = RTN_FindByAddress(IMG_LowAddress(img) + SYM_Value(sym));
        if (RTN_Valid(rtn)) {
            const auto& func = RTN_Name(rtn);
            string fullFunctionName = toUpperCase(moduleName) + ":" + func;
            if (fullFunctionName == "ADVAPI32.DLL:RegQueryValueExW") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstRegQueryValueEx::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "NTDLL.DLL:NtQueryInformationProcess") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstNtQueryInformationProcess::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "ADVAPI32.DLL:RegOpenKeyExW") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstRegOpenKeyEx::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:GetModuleHandleA") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstGetModuleHandleA::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:GetModuleHandleW") {
                cout << "Instrumentando " << fullFunctionName << endl;
                //InstGetModuleHandleW::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:GetModuleFileNameA") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstGetModuleFileNameA::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:IsDebuggerPresent") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstIsDebuggerPresent::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:CheckRemoteDebuggerPresent") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstCheckRemoteDebuggerPresent::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:OutputDebugString") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstOutputDebugString::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:QueryPerformanceCounter") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstQueryPerformanceCounter::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "USER32.dll:BlockInput") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstBlockInput::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "USER32.dll:GetAsyncKeyState") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstGetAsyncKeyState::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "USER32.dll:GetForegroundWindow") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstGetForegroundWindow::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "KERNEL32.DLL:GetTickCount") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstGetTickCount::InstrumentFunction(rtn);
            }
            else if (fullFunctionName == "NTDLL.DLL:NtQuerySystemInformation") {
                cout << "Instrumentando " << fullFunctionName << endl;
                InstNtQuerySystemInformation::InstrumentFunction(rtn);
            }
        }
    }

}

int main(int argc, char* argv[]) {
    FillInterceptSet();
    
    PIN_Init(argc, argv);
    TraceFile.open("TraceFile.txt");
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(InstrumentFunctions, 0);
    PIN_StartProgram();
    
    return 0;
}