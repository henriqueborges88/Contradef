#include "pin.H"
#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include "InstrumentationContext.h"

using std::string;
using std::cerr;
using std::cout;
using std::wcerr;
using std::endl;
using std::flush;
using std::set;
using std::wstring;


std::ofstream TraceFile;

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

    
            "module": "KERNEL32.DLL",
            "function": "GetProcAddress",
            "address": "0x7ffc3e4c3c10"
        },
        {
            "module": "KERNEL32.DLL",
            "function": "GetCurrentProcess",
            "address": "0x7ffc3e4d0160"
        },

        Themida
         {
            "module": "KERNEL32.DLL",
            "function": "GetProcessHeap",
            "address": "0x7ffc3e4c0dd0"
        },
        {
            "module": "KERNEL32.DLL",
            "function": "GetModuleFileNameW",
            "address": "0x7ffc3e4c7030"
        },
         {
            "module": "KERNEL32.DLL",
            "function": "GetCurrentThreadId",
            "address": "0x7ffc3e4b2750"
        },
        {
            "module": "KERNEL32.DLL",
            "function": "GetCommandLineA",
            "address": "0x7ffc3e4c8ad0"
        },
           {
            "module": "KERNEL32.DLL",
            "function": "LoadLibraryExW",
            "address": "0x7ffc3e4c3d90"
        },
        ZwQueryInformationThread
        ZwOpenProcess
        NtOpenProcess
        kernelbase.IsDebuggerPresent
        kernelbase.DebugBreak
        kernelbase.IsDBCSLeadByte
        kernelbase.GetEnvironmentStringsW <- envs do DBI?
        kernelbase.ExpandEnvironmentStringsW
        kernel32.ProcessIdToSessionId
        kernel32.GetProcessTimes
        kernel32.GetCurrentThreadId
        kernel32.GetCurrentProcess
        kernel32.GetCurrentProcessId
        ntdll.QueryPerformanceCounter
        kernelbase.RegCloseKey
        kernelbase.RegOpenKeyExW

        obsedium:
        Export OptionalHeader.AddressOfEntryPoint <_ verificar isso
        shell32.ShellAboutW
        user32.CharNextA
        advapi32.RegisterEventSourceA

        EtwLogTraceEvent de ntdll.dll
    */

InstrumentationContext context; // As estratégias já estão adicionadas no contexto

VOID InstrumentFunctions(IMG img, VOID* v) {
    context.ExecuteAllStrategies(img);
}


int main(int argc, char* argv[]) {
    PIN_Init(argc, argv);
    TraceFile.open("TraceFile.txt");
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(InstrumentFunctions, 0);
    PIN_StartProgram();
    
    return 0;
}