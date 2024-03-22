#include "InstrumentationContext.h"
#include "Notifier.h"


InstrumentationContext::InstrumentationContext() {
    //TODO: Alterar para o modo de instrumentação Probed (sonda)

    /*
    
    * Com Problema
    strategyMap["QueryPerformanceCounter"] = &InstQueryPerformanceCounter::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado. O CB After funciona no modo Probed
    strategyMap["QueryPerformanceFrequency"] = &InstQueryPerformanceFrequency::InstrumentFunction;  // <-----Verificar, o CB after não está sendo invocado. O CB After funciona no modo Probed


    * Falta implementar a lógica de detecção
    strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
    strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
    strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
    strategyMap["VirtualQuery"] = &InstVirtualQuery::InstrumentFunction;
    strategyMap["VirtualQueryEx"] = &InstVirtualQueryEx::InstrumentFunction;
    strategyMap["VirtualProtect"] = &InstVirtualProtect::InstrumentFunction;
    strategyMap["VirtualProtectEx"] = &InstVirtualProtectEx::InstrumentFunction;

    
    */

    
    strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
    strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction;
    strategyMap["GetModuleFileNameExA"] = &InstGetModuleFileNameExA::InstrumentFunction;
    strategyMap["GetModuleFileNameExW"] = &InstGetModuleFileNameExW::InstrumentFunction;
    strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
    strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
    strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
    strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
    strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;
    strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
    strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
    strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
    strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
    strategyMap["RegQueryValueA"] = &InstRegQueryValueA::InstrumentFunction;
    strategyMap["RegQueryValueW"] = &InstRegQueryValueW::InstrumentFunction;
    strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
    strategyMap["NtQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
    strategyMap["GetWindowTextA"] = &InstGetWindowTextA::InstrumentFunction;
    strategyMap["GetWindowTextW"] = &InstGetWindowTextW::InstrumentFunction;
    strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
    strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
    strategyMap["OutputDebugStringW"] = &InstOutputDebugStringW::InstrumentFunction;
    strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    
    // Com Problema, não chama o after -> strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado, já tentei removendo os parametros. O CB After funciona no modo Probed


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
    for (const auto& pair : strategyMap) {
        RTN rtn = RTN_FindByName(img, pair.first.c_str());
        if (RTN_Valid(rtn)) {
            RTNFunction func = pair.second;
            func(rtn, globalNotifier);
        }
    }
}
