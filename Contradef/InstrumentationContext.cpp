#include "InstrumentationContext.h"
#include "Notifier.h"

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

InstrumentationContext::InstrumentationContext() {
    // Contrutor
}

InstrumentationContext::~InstrumentationContext() {
    // Não é necessário deletar os ponteiros de função

    scanScope.clear(); // Limpa a memória de scanScope ao destruir o objeto
    strategyMap.clear();  // Limpa o mapa de estratégias
}

bool InstrumentationContext::IsStringInScanScope(const std::string& scope) const {
    return std::find(scanScope.begin(), scanScope.end(), scope) != scanScope.end();
}

void InstrumentationContext::InitStrategies() {

    




    for (const std::string& scope : scanScope) {
        if (scope == "Obsidium") {
            std::cout << "Instrumentando o Obsidium" << std::endl;
            //strategyMap["CreateFileW"] = &InstCreateFileW::InstrumentFunction;
            //strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
            //strategyMap["ZwQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
        }

        if (scope == "Themida") {
            //Estrategias do Themida
        }

        if (scope == "VM") {
            //Estrategias de virtualizaçã
        }
    }
    
    


    
    
    
    /*strategyMap["lstrcpynA"] = &InstLstrcpynA::InstrumentFunction;
    strategyMap["lstrcpy"] = &InstLstrcpy::InstrumentFunction;
    strategyMap["WriteConsoleW"] = &InstWriteConsoleW::InstrumentFunction;
    strategyMap["ExitProcess"] = &InstExitProcess::InstrumentFunction;
    strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["GetModuleHandleW"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["DeleteFileA"] = &InstDeleteFileA::InstrumentFunction;
    strategyMap["ShellExecute"] = &InstShellExecute::InstrumentFunction;
    strategyMap["ShellExecuteEx"] = &InstShellExecuteEx::InstrumentFunction;*/
    
    
    // Com Problema, não chama o after -> strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado, já tentei removendo os parametros. O CB After funciona no modo Probed


    // Registro do Windows
    //strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
    //strategyMap["BaseRegEnumKey"] = &InstBaseRegEnumKey::InstrumentFunction;
    //strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
    //strategyMap["RegEnumKeyA"] = &InstRegEnumKeyA::InstrumentFunction;
    //strategyMap["RegQueryValueA"] = &InstRegQueryValueA::InstrumentFunction;
    //strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;
    //strategyMap["RegEnumKeyW"] = &InstRegEnumKeyW::InstrumentFunction;
    //strategyMap["RegQueryValueW"] = &InstRegQueryValueW::InstrumentFunction;
    //strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
    //strategyMap["RegEnumKeyExA"] = &InstRegEnumKeyExA::InstrumentFunction;
    //strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
    //strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
    //strategyMap["RegEnumKeyExW"] = &InstRegEnumKeyExW::InstrumentFunction;
    //strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
   // //strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
   // //strategyMap["ZwOpenKey"] = &InstNtOpenKey::InstrumentFunction;
   // strategyMap["NtEnumKey"] = &InstNtEnumKey::InstrumentFunction;
   // strategyMap["ZwEnumKey"] = &InstNtEnumKey::InstrumentFunction;
   // //strategyMap["NtQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
   // //strategyMap["ZwQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
   // strategyMap["NtQueryKey"] = &InstNtQueryKey::InstrumentFunction;
   // strategyMap["ZwQueryKey"] = &InstNtQueryKey::InstrumentFunction;


   //// // Experimentando
   // strategyMap["CoCreateInstance"] = &InstWmi::InstrumentFunction;
   // strategyMap["ConnectServer"] = &InstWmi::InstrumentFunction;
   // strategyMap["ExecQuery"] = &InstWmi::InstrumentFunction;
   // strategyMap["CoCreateInstanceEx"] = &InstWmiEx::InstrumentFunction;
   // strategyMap["ConnectServer"] = &InstWmiEx::InstrumentFunction;
   // strategyMap["ExecQuery"] = &InstWmiEx::InstrumentFunction;
   // strategyMap["GetObject"] = &InstWmiEx::InstrumentFunction;
   // strategyMap["GetObjectA"] = &InstWmiEx::InstrumentFunction;
   // strategyMap["GetObjectW"] = &InstWmiEx::InstrumentFunction;
    //strategyMap["GetWriteWatch"] = &InstGetWriteWatch::InstrumentFunction;
   //// // Fin Experimentando

   //// 
   //// // Falta implementar a lógica de detecção
   // strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
   // strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
   // strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction;
   // strategyMap["GetModuleFileNameExA"] = &InstGetModuleFileNameExA::InstrumentFunction;
   // strategyMap["GetModuleFileNameExW"] = &InstGetModuleFileNameExW::InstrumentFunction;
   // strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
   // strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
   // strategyMap["GetWindowTextA"] = &InstGetWindowTextA::InstrumentFunction;
   // strategyMap["GetWindowTextW"] = &InstGetWindowTextW::InstrumentFunction;
   // strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
   // strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
   // strategyMap["OutputDebugStringW"] = &InstOutputDebugStringW::InstrumentFunction;
   // strategyMap["FindWindowA"] = &InstFindWindowA::InstrumentFunction;
   // strategyMap["FindWindowW"] = &InstFindWindowW::InstrumentFunction;
   // strategyMap["KiUserExceptionDispatcher"] = &InstKiUserExceptionDispatcher::InstrumentFunction;
   // strategyMap["GetTickCount64"] = &InstGetTickCount64::InstrumentFunction;
   // strategyMap["GetLocalTime"] = &InstGetLocalTime::InstrumentFunction;
   // strategyMap["GetSystemTime"] = &InstGetSystemTime::InstrumentFunction;
   // strategyMap["OutputDebugStringA"] = &InstOutputDebugStringA::InstrumentFunction;
   // strategyMap["BlockInput"] = &InstBlockInput::InstrumentFunction;
   // strategyMap["OpenProcess"] = &InstOpenProcess::InstrumentFunction;
   // strategyMap["CreateProcess"] = &InstCreateProcess::InstrumentFunction;
   // strategyMap["CreateProcessAsUser"] = &InstCreateProcessAsUser::InstrumentFunction;
   // strategyMap["ShellExecute"] = &InstShellExecute::InstrumentFunction;
   //// strategyMap["ShellExecuteEx"] = &InstShellExecuteEx::InstrumentFunction;
   // strategyMap["WinExec"] = &InstWinExec::InstrumentFunction;
   // strategyMap["CreateService"] = &InstCreateService::InstrumentFunction;
   // strategyMap["StartService"] = &InstStartService::InstrumentFunction;
   // strategyMap["CoCreateInstance"] = &InstCoCreateInstance::InstrumentFunction;
   // strategyMap["LoadLibrary"] = &InstLoadLibrary::InstrumentFunction;
   // strategyMap["LoadLibraryEx"] = &InstLoadLibraryEx::InstrumentFunction;
   // strategyMap["NtOpenProcess"] = &InstNtOpenProcess::InstrumentFunction;
   // strategyMap["NtSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
   // strategyMap["ZwSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
   // strategyMap["NtSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
   // strategyMap["ZwSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
   // strategyMap["NtQueryObject"] = &InstNtQueryObject::InstrumentFunction;
   // strategyMap["ZwQueryObject"] = &InstNtQueryObject::InstrumentFunction;
   // strategyMap["NtYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
   // strategyMap["ZwYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
   // strategyMap["NtCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
   // strategyMap["ZwCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
   // strategyMap["NtCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
   // strategyMap["ZwCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
   // strategyMap["NtUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
   // strategyMap["ZwUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
   // /*strategyMap["NtUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;
   // strategyMap["ZwUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;*/
   // strategyMap["NtUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
   // strategyMap["ZwUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
   // strategyMap["NtSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
   // strategyMap["ZwSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
   // strategyMap["NtClose"] = &InstNtClose::InstrumentFunction;
   // strategyMap["ZwClose"] = &InstNtClose::InstrumentFunction;
   // strategyMap["NtGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
   // strategyMap["ZwGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
   // strategyMap["NtSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
   // strategyMap["ZwSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
   // strategyMap["NtContinue"] = &InstNtContinue::InstrumentFunction;
   // strategyMap["ZwContinue"] = &InstNtContinue::InstrumentFunction;
   // strategyMap["NtQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
   // strategyMap["ZwQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
   // strategyMap["NtQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
   // strategyMap["ZwQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
   // strategyMap["NtQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;
   // strategyMap["ZwQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;
   // strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
   // strategyMap["ZwQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
   // strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
   // strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
   // strategyMap["VirtualProtect"] = &InstVirtualProtect::InstrumentFunction;
   // strategyMap["VirtualProtectEx"] = &InstVirtualProtectEx::InstrumentFunction;
   // //strategyMap["VirtualQuery"] = &InstVirtualQuery::InstrumentFunction;
   // //strategyMap["VirtualQueryEx"] = &InstVirtualQueryEx::InstrumentFunction;
   // strategyMap["QueryPerformanceCounter"] = &InstQueryPerformanceCounter::InstrumentFunction;
   // strategyMap["NtQueryPerformanceCounter"] = &InstNtQueryPerformanceCounter::InstrumentFunction;
   // strategyMap["ZwQueryPerformanceCounter"] = &InstNtQueryPerformanceCounter::InstrumentFunction;

}

void InstrumentationContext::ExecuteStrategy(const std::string& name, RTN rtn, Notifier& globalNotifier) {
    auto it = strategyMap.find(name);
    if (it != strategyMap.end()) {
        (*it->second)(rtn, globalNotifier);
    }
}

bool InstrumentationContext::HasStrategy(const std::string& name) const {
    return strategyMap.find(name) != strategyMap.end();
}

void InstrumentationContext::ExecuteAllStrategies(IMG img, Notifier& globalNotifier) {

    std::string moduleName = IMG_Name(img);
    for (SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {
        RTN rtn = RTN_FindByAddress(IMG_LowAddress(img) + SYM_Value(sym));
        if (RTN_Valid(rtn)) {
            std::string funcName = RTN_Name(rtn);
            for (const auto& pair : strategyMap) {
                std::string strategyName = pair.first.c_str();
                if (strategyName == funcName) {
                    RTNFunction func = pair.second;
                    func(rtn, globalNotifier);
                }
            }
        }
    }

}