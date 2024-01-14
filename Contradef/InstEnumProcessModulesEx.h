#pragma once
#ifndef INST_ENUM_PROCESS_MODULES_EX_H
#define INST_ENUM_PROCESS_MODULES_EX_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstEnumProcessModulesExArgs {
    ADDRINT hProcess;
    ADDRINT lphModule;
    ADDRINT cb;
    ADDRINT lpcbNeeded;
    ADDRINT dwFilterFlag;
};

class InstEnumProcessModulesEx : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstEnumProcessModulesExArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT lphModule, ADDRINT cb, ADDRINT lpcbNeeded, ADDRINT dwFilterFlag);
};

#endif // INST_ENUM_PROCESS_MODULES_EX_H
