#include "pin.H"
#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include <unistd.h>
#include <deque>
#include <map>
#include "InstrumentationContext.h"
#include "Notifier.h"

using std::string;
using std::cerr;
using std::cout;
using std::wcerr;
using std::endl;
using std::flush;
using std::set;
using std::wstring;

PIN_LOCK lock;
std::ofstream OutFile;
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "contradef.out", "specify file name for insmix profile ");
InstrumentationContext context; // As estratégias já estão adicionadas no contexto

Notifier globalNotifier;

/*
EnumProcessModules/EnumProcessModulesEx: Essas funções podem ser usadas para enumerar os módulos carregados em um processo. Isso é útil para detectar DLLs suspeitas ou inesperadas que podem ser injetadas por instrumentadores binários.

GetModuleHandle/GetModuleFileName: Essas funções ajudam a obter handles e nomes de arquivos de módulos específicos. Você pode usá-las para verificar se um módulo conhecido de um instrumentador DBI, como o dynamorio.dll (usado pelo DynamoRIO) ou pinvm.dll (usado pelo Intel PIN), está carregado em um processo.

VirtualQuery/VirtualProtect: Estas funções permitem inspecionar e modificar as permissões de páginas de memória. Instrumentadores DBI frequentemente modificam permissões de memória para injetar código ou realizar a instrumentação. GetModuleHandle ou EnumProcessModules podem tentar ver se VirtualQuery/VirtualProtect estão carregadas

CreateToolhelp32Snapshot/Module32First/Module32Next: Estas funções são usadas para tirar um 'snapshot' dos processos e módulos em execução no sistema. Eles podem ser usados para detectar alterações suspeitas no espaço de endereço de um processo.

Debug API (como DebugActiveProcess): A API de Debug do Windows pode ser utilizada para anexar a um processo e monitorar suas atividades, o que pode revelar a presença de DBIs.

Event Tracing for Windows (ETW): O ETW fornece um mecanismo de rastreamento de eventos de alto desempenho. Você pode usar o ETW para monitorar eventos do sistema e de aplicativos que podem indicar a presença de instrumentação binária.

NtQueryInformationProcess: Esta função de baixo nível pode ser usada para obter informações detalhadas sobre um processo, incluindo a presença de hooks ou alterações suspeitas no espaço de endereço.

System Service Descriptor Table (SSDT) Hooking: Verificar alterações no SSDT, pois os DBIs podem modificar esta tabela para interceptar chamadas de sistema.

    
            "module": "KERNEL32.DLL",
            "function": "GetProcAddress",
            "address": "0x7ffc3e4c3c10"
     
            "module": "KERNEL32.DLL",
            "function": "GetCurrentProcess",
            "address": "0x7ffc3e4d0160"

        Themida
       
            "module": "KERNEL32.DLL",
            "function": "GetCurrentThreadId",
            "address": "0x7ffc3e4b2750"
        
            "module": "KERNEL32.DLL",
            "function": "GetCommandLineA",
            "address": "0x7ffc3e4c8ad0"
        
            "module": "KERNEL32.DLL",
            "function": "LoadLibraryExW",
            "address": "0x7ffc3e4c3d90"

        ZwQueryInformationThread
        ZwOpenProcess
        NtOpenProcess
        kernelbase.DebugBreak
        kernelbase.IsDBCSLeadByte
        kernelbase.GetEnvironmentStringsW <- envs do DBI?
        kernelbase.ExpandEnvironmentStringsW
        kernel32.ProcessIdToSessionId
        kernel32.GetProcessTimes
        kernel32.GetCurrentThreadId
        kernel32.GetCurrentProcess
        kernel32.GetCurrentProcessId

        obsedium:
        Export OptionalHeader.AddressOfEntryPoint <_ verificar isso
        shell32.ShellAboutW
        user32.CharNextA
        advapi32.RegisterEventSourceA

        EtwLogTraceEvent de ntdll.dll
    */

INT32 Usage()
{
    cerr << "Esta ferramenta PIN fornece informação sobre possíveis comportamentos evasivos (antianálise) presentes em executáveis\n"
        "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

VOID InstrumentFunctions(IMG img, VOID* v) {
    context.ExecuteAllStrategies(img, globalNotifier);
}

VOID HandleExecutionCompletedEvent(const EventData* data, void* context) {
    if (data->type != EventData::ExecutionCompleted) {
        return;
    }
    auto executionEvent = static_cast<const ExecutionEventData*>(data);
    auto executionOutputText = executionEvent->executionInformation.outputText;
    OutFile << executionOutputText;
    OutFile.flush();  // Esvazia o buffer de saída
}

int main(int argc, char* argv[]) {

    string filename;
    filename = KnobOutputFile.Value();
    filename += "." + decstr(getpid()) + ".txt";
    OutFile.open(filename.c_str());

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    // Criando observadores dinamicamente com o contexto nulo
    auto* executionCompleted = new Observer(HandleExecutionCompletedEvent, NULL);

    // Anexa os observers ao notificador
    globalNotifier.Attach(executionCompleted);
    
    PIN_InitLock(&lock);

    PIN_InitSymbols();

    IMG_AddInstrumentFunction(InstrumentFunctions, 0);

    PIN_StartProgram();

    OutFile.close();
    return 0;
}
