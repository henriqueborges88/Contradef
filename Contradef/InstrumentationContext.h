#pragma once
#ifndef INSTRUMENTATION_CONTEXT_H
#define INSTRUMENTATION_CONTEXT_H

#include <iostream>
#include "InstrumentationStrategy.h"
#include <map>
#include <string>
#include <functional>
#include <deque>
#include "InstIsDebuggerPresent.h"
#include "InstCheckRemoteDebuggerPresent.h"
#include "InstOutputDebugStringW.h"
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
#include "InstVirtualQuery.h"
#include "InstVirtualQueryEx.h"
#include "InstVirtualProtect.h"
#include "InstVirtualProtectEx.h"

typedef void (*RTNFunction)(RTN, Notifier&);

class InstrumentationContext {
private:
    std::map<std::string, RTNFunction> strategyMap;

public:
    InstrumentationContext();
    ~InstrumentationContext();
    void ExecuteStrategy(const std::string& name, RTN rtn, Notifier& globalNotifier);
    bool HasStrategy(const std::string& name) const;
    void ExecuteAllStrategies(IMG img, Notifier& globalNotifier);
};

#endif // INSTRUMENTATION_CONTEXT_H