// InstrumentationContext.cpp
#include "InstrumentationContext.h"
#include "InstRegQueryValueExW.h"
#include "InstRegOpenKeyExW.h"
#include "InstRegOpenKeyExA.h"
#include "InstBaseRegOpenKey.h"
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
#include "InstNtOpenKey.h"
#include "InstEnumProcessModules.h"
#include "InstEnumProcessModulesEx.h"
#include "InstRegOpenKeyA.h"
#include "InstRegOpenKeyW.h"
#include "InstGetModuleFileNameW.h"

InstrumentationContext::InstrumentationContext() {

    strategyMap["RegQueryValueExW"] = &InstRegQueryValueExW::InstrumentFunction;
    strategyMap["NtQueryInformationProcess"] = &InstNtQueryInformationProcess::InstrumentFunction;
    strategyMap["BaseRegOpenKey"] = &InstBaseRegOpenKey::InstrumentFunction;
    strategyMap["RegOpenKeyExW"] = &InstRegOpenKeyExW::InstrumentFunction;
    strategyMap["RegOpenKeyExA"] = &InstRegOpenKeyExA::InstrumentFunction;
    //strategyMap["RegOpenKeyA"] = &InstRegOpenKeyA::InstrumentFunction;  <---- verificar esse
    //strategyMap["RegOpenKeyW"] = &InstRegOpenKeyW::InstrumentFunction;  <---- verificar esse
    strategyMap["GetModuleHandleA"] = &InstGetModuleHandleA::InstrumentFunction;
    //strategyMap["GetModuleHandleW"] = &InstGetModuleHandleW::InstrumentFunction; <---- verificar esse
    strategyMap["GetModuleFileNameA"] = &InstGetModuleFileNameA::InstrumentFunction;
    //strategyMap["GetModuleFileNameW"] = &InstGetModuleFileNameW::InstrumentFunction; <---- verificar esse
    strategyMap["IsDebuggerPresent"] = &InstIsDebuggerPresent::InstrumentFunction;
    strategyMap["CheckRemoteDebuggerPresent"] = &InstCheckRemoteDebuggerPresent::InstrumentFunction;
    strategyMap["OutputDebugString"] = &InstOutputDebugString::InstrumentFunction;
    strategyMap["QueryPerformanceCounter"] = &InstQueryPerformanceCounter::InstrumentFunction;
    strategyMap["BlockInput"] = &InstBlockInput::InstrumentFunction;
    strategyMap["GetAsyncKeyState"] = &InstGetAsyncKeyState::InstrumentFunction;
    strategyMap["GetForegroundWindow"] = &InstGetForegroundWindow::InstrumentFunction;
    strategyMap["GetTickCount"] = &InstGetTickCount::InstrumentFunction;
    strategyMap["NtQuerySystemInformation"] = &InstNtQuerySystemInformation::InstrumentFunction;
    strategyMap["NtOpenKey"] = &InstNtOpenKey::InstrumentFunction;
    strategyMap["EnumProcessModules"] = &InstEnumProcessModules::InstrumentFunction;
    strategyMap["EnumProcessModulesEx"] = &InstEnumProcessModulesEx::InstrumentFunction;
}

InstrumentationContext::~InstrumentationContext() {
    // Não é necessário deletar os ponteiros de função

    strategyMap.clear();  // Opcional, limpa o mapa
}

void InstrumentationContext::ExecuteStrategy(const std::string& name, RTN rtn) {
    auto it = strategyMap.find(name);
    if (it != strategyMap.end()) {
        std::cout << "Instrumentando a função " << name << std::endl;
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
