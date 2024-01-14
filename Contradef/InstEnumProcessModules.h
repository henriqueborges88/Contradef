#pragma once
#ifndef INST_ENUM_PROCESS_MODULES_H
#define INST_ENUM_PROCESS_MODULES_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstEnumProcessModulesArgs {
    ADDRINT hProcess;
    ADDRINT lphModule;
    ADDRINT cb;
    ADDRINT lpcbNeeded;
};

class InstEnumProcessModules : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstEnumProcessModulesArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded);
};

#endif // INST_ENUM_PROCESS_MODULES_H
