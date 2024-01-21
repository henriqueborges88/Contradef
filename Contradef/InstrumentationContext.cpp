// InstrumentationContext.cpp
#include "InstrumentationContext.h"
#include "InstIsDebuggerPresent.h"
#include "InstCheckRemoteDebuggerPresent.h"
#include "InstOutputDebugStringW.h"
#include "InstBlockInput.h"
#include "InstGetForegroundWindow.h"
#include "InstNtQueryInformationProcess.h"
#include "InstNtQuerySystemInformation.h"
#include "InstEnumProcessModules.h"
#include "InstEnumProcessModulesEx.h"
#include "InstBaseRegOpenKey.h"
#include "InstRegOpenKeyW.h"
#include "InstRegOpenKeyA.h"
#include "InstRegOpenKeyExW.h"
#include "InstRegOpenKeyExA.h"
#include "InstRegQueryValueW.h"
#include "InstRegQueryValueA.h"
#include "InstRegQueryValueExW.h"
#include "InstRegQueryValueExA.h"
#include "InstNtOpenKey.h"
#include "InstNtQueryValueKey.h"
#include "InstGetModuleHandleW.h"
#include "InstGetModuleHandleA.h"
#include "InstGetModuleFileNameW.h"
#include "InstGetModuleFileNameA.h"
#include "InstGetModuleFileNameExW.h"
#include "InstGetModuleFileNameExA.h"
#include "InstGetWindowTextW.h"
#include "InstGetWindowTextA.h"
#include "InstQueryPerformanceCounter.h"
#include "InstQueryPerformanceFrequency.h"
#include "InstGetTickCount.h"


InstrumentationContext::InstrumentationContext() {
    //strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
    //strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
    //strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
    //strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado, já tentei removendo os parametros. O CB After funciona no modo Probed
    //strategyMap["QueryPerformanceCounter"] = &InstQueryPerformanceCounter::InstrumentFunction; // <-----Verificar, o CB after não está sendo invocado. O CB After funciona no modo Probed
    
    strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
    strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
    strategyMap["OutputDebugStringW"] = &InstOutputDebugStringW::InstrumentFunction;
    strategyMap["BlockInput"] = &InstBlockInput::InstrumentFunction;
    strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
    strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;
    strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;
    strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
    strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
    strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
    strategyMap["RegQueryValueExA"] = &InstRegQueryValueExA::InstrumentFunction;
    strategyMap["RegQueryValueA"] = &InstRegQueryValueA::InstrumentFunction;
    strategyMap["RegQueryValueW"] = &InstRegQueryValueW::InstrumentFunction;
    strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
    strategyMap["NtQueryValueKey"] = &InstNtQueryValueKey::InstrumentFunction;
    strategyMap["GetWindowTextW"] = &InstGetWindowTextW::InstrumentFunction; 
    strategyMap["GetWindowTextA"] = &InstGetWindowTextA::InstrumentFunction; 
    strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction;
    strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction;
    strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
    strategyMap["GetModuleFileNameExA"] = &InstGetModuleFileNameExA::InstrumentFunction; 
    strategyMap["GetModuleFileNameExW"] = &InstGetModuleFileNameExW::InstrumentFunction; 
    strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
    strategyMap["QueryPerformanceFrequency"] = &InstQueryPerformanceFrequency::InstrumentFunction;
}

InstrumentationContext::~InstrumentationContext() {
    // Não é necessário deletar os ponteiros de função

    strategyMap.clear();  // Opcional, limpa o mapa
}

void InstrumentationContext::ExecuteStrategy(const std::string& name, RTN rtn) {
    auto it = strategyMap.find(name);
    if (it != strategyMap.end()) {
        std::cout << "[Contradef] Instrumentando a função " << name << std::endl;
        (*it->second)(rtn);
    }
}

bool InstrumentationContext::HasStrategy(const std::string& name) const {
    return strategyMap.find(name) != strategyMap.end();
}

void InstrumentationContext::ExecuteAllStrategies(IMG img) {
    std::string moduleName = IMG_Name(img);
    for (const auto& pair : strategyMap) {
        RTN rtn = RTN_FindByName(img, pair.first.c_str());
        if (RTN_Valid(rtn)) {
            std::cout << "Executando estratégia: " << pair.first << " no modulo " << moduleName << std::endl;
            RTNFunction func = pair.second;
            func(rtn);
        }
    }
}
