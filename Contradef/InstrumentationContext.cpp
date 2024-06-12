#include "InstrumentationContext.h"
#include "Notifier.h"


InstrumentationContext::InstrumentationContext() {
    //TODO: Alterar para o modo de instrumentação Probed (sonda)

    /*
    
    * Com Problema
    // Com Problema, não chama o after -> strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado, já tentei removendo os parametros. O CB After funciona no modo Probed

    * Falta implementar a lógica de detecção
    strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
    strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
    strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
    strategyMap["VirtualQuery"] = &InstVirtualQuery::InstrumentFunction;
    strategyMap["VirtualQueryEx"] = &InstVirtualQueryEx::InstrumentFunction;
    strategyMap["VirtualProtect"] = &InstVirtualProtect::InstrumentFunction;
    strategyMap["VirtualProtectEx"] = &InstVirtualProtectEx::InstrumentFunction;

    
    */

    
    //strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
    //strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    //strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction;
    //strategyMap["GetModuleFileNameExA"] = &InstGetModuleFileNameExA::InstrumentFunction;
    //strategyMap["GetModuleFileNameExW"] = &InstGetModuleFileNameExW::InstrumentFunction;
    //strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
    //strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
    //strategyMap["GetWindowTextA"] = &InstGetWindowTextA::InstrumentFunction;
    //strategyMap["GetWindowTextW"] = &InstGetWindowTextW::InstrumentFunction;
    //strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
    //strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
    //strategyMap["OutputDebugStringW"] = &InstOutputDebugStringW::InstrumentFunction;
    //strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    
    
    // Experimentando
    /*
    strategyMap["CoCreateInstance"] = &InstWmi::InstrumentFunction;
    strategyMap["ConnectServer"] = &InstWmi::InstrumentFunction;
    strategyMap["ExecQuery"] = &InstWmi::InstrumentFunction;

    strategyMap["CoCreateInstanceEx"] = &InstWmiEx::InstrumentFunction;
    strategyMap["ConnectServer"] = &InstWmiEx::InstrumentFunction;
    strategyMap["ExecQuery"] = &InstWmiEx::InstrumentFunction;
    strategyMap["GetObject"] = &InstWmiEx::InstrumentFunction;
    strategyMap["GetObjectA"] = &InstWmiEx::InstrumentFunction;
    strategyMap["GetObjectW"] = &InstWmiEx::InstrumentFunction;
    */

    // Registro do Windows
    /*strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
    strategyMap["BaseRegEnumKey"] = &InstBaseRegEnumKey::InstrumentFunction;
    strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
    strategyMap["RegEnumKeyA"] = &InstRegEnumKeyA::InstrumentFunction;
    strategyMap["RegQueryValueA"] = &InstRegQueryValueA::InstrumentFunction;
    strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;
    strategyMap["RegEnumKeyW"] = &InstRegEnumKeyW::InstrumentFunction;
    strategyMap["RegQueryValueW"] = &InstRegQueryValueW::InstrumentFunction;
    strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
    strategyMap["RegEnumKeyExA"] = &InstRegEnumKeyExA::InstrumentFunction;
    strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
    strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
    strategyMap["RegEnumKeyExW"] = &InstRegEnumKeyExW::InstrumentFunction;
    strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
    strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
    strategyMap["ZwOpenKey"] = &InstNtOpenKey::InstrumentFunction;
    strategyMap["NtEnumKey"] = &InstNtEnumKey::InstrumentFunction;
    strategyMap["ZwEnumKey"] = &InstNtEnumKey::InstrumentFunction;
    strategyMap["NtQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;*/
    strategyMap["ZwQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
    strategyMap["NtQueryKey"] = &InstNtQueryKey::InstrumentFunction;


    /*
    strategyMap["FindWindowA"] = &InstFindWindowA::InstrumentFunction;
    strategyMap["FindWindowW"] = &InstFindWindowW::InstrumentFunction;
    strategyMap["KiUserExceptionDispatcher"] = &InstKiUserExceptionDispatcher::InstrumentFunction;
    strategyMap["GetTickCount64"] = &InstGetTickCount64::InstrumentFunction;
    strategyMap["GetLocalTime"] = &InstGetLocalTime::InstrumentFunction;
    strategyMap["GetSystemTime"] = &InstGetSystemTime::InstrumentFunction;
    strategyMap["OutputDebugStringA"] = &InstOutputDebugStringA::InstrumentFunction;
    strategyMap["BlockInput"] = &InstBlockInput::InstrumentFunction;
    strategyMap["OpenProcess"] = &InstOpenProcess::InstrumentFunction;
    strategyMap["CreateProcess"] = &InstCreateProcess::InstrumentFunction;
    strategyMap["CreateProcessAsUser"] = &InstCreateProcessAsUser::InstrumentFunction;
    strategyMap["ShellExecute"] = &InstShellExecute::InstrumentFunction;
    strategyMap["ShellExecuteEx"] = &InstShellExecuteEx::InstrumentFunction;
    strategyMap["WinExec"] = &InstWinExec::InstrumentFunction;
    strategyMap["CreateService"] = &InstCreateService::InstrumentFunction;
    strategyMap["StartService"] = &InstStartService::InstrumentFunction;
    strategyMap["CoCreateInstance"] = &InstCoCreateInstance::InstrumentFunction;
    strategyMap["LoadLibrary"] = &InstLoadLibrary::InstrumentFunction;
    strategyMap["LoadLibraryEx"] = &InstLoadLibraryEx::InstrumentFunction;
    strategyMap["NtOpenProcess"] = &InstNtOpenProcess::InstrumentFunction;
    strategyMap["NtSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
    strategyMap["ZwSetInformationThread"] = &InstNtSetInformationThread::InstrumentFunction;
    strategyMap["NtSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
    strategyMap["ZwSetInformationProcess"] = &InstNtSetInformationProcess::InstrumentFunction;
    strategyMap["NtQueryObject"] = &InstNtQueryObject::InstrumentFunction;
    strategyMap["ZwQueryObject"] = &InstNtQueryObject::InstrumentFunction;
    strategyMap["NtYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
    strategyMap["ZwYieldExecution"] = &InstNtYieldExecution::InstrumentFunction;
    strategyMap["NtCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
    strategyMap["ZwCreateProcessEx"] = &InstNtCreateProcessEx::InstrumentFunction;
    strategyMap["NtCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
    strategyMap["ZwCreateThreadEx"] = &InstNtCreateThreadEx::InstrumentFunction;
    strategyMap["NtUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
    strategyMap["ZwUserFindWindowEx"] = &InstNtUserFindWindowEx::InstrumentFunction;
    strategyMap["NtUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;
    strategyMap["ZwUserBuildHwndList"] = &InstNtUserBuildHwndList::InstrumentFunction;
    strategyMap["NtUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
    strategyMap["ZwUserQueryWindow"] = &InstNtUserQueryWindow::InstrumentFunction;
    strategyMap["NtSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
    strategyMap["ZwSetDebugFilterState"] = &InstNtSetDebugFilterState::InstrumentFunction;
    strategyMap["NtClose"] = &InstNtClose::InstrumentFunction;
    strategyMap["ZwClose"] = &InstNtClose::InstrumentFunction;
    strategyMap["NtGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
    strategyMap["ZwGetContextThread"] = &InstNtGetContextThread::InstrumentFunction;
    strategyMap["NtSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
    strategyMap["ZwSetContextThread"] = &InstNtSetContextThread::InstrumentFunction;
    strategyMap["NtContinue"] = &InstNtContinue::InstrumentFunction;
    strategyMap["ZwContinue"] = &InstNtContinue::InstrumentFunction;
    strategyMap["NtQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
    strategyMap["ZwQuerySystemTime"] = &InstNtQuerySystemTime::InstrumentFunction;
    strategyMap["NtQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
    strategyMap["ZwQueryInformationThread"] = &InstNtQueryInformationThread::InstrumentFunction;
    strategyMap["NtQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;
    strategyMap["ZwQueryVirtualMemory"] = &InstNtQueryVirtualMemory::InstrumentFunction;

    */


}

InstrumentationContext::~InstrumentationContext() {
    // Não é necessário deletar os ponteiros de função

    strategyMap.clear();  // Opcional, limpa o mapa
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