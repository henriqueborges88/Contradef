#pragma once
#ifndef INST_NT_OPEN_KEY_H
#define INST_NT_OPEN_KEY_H

#include "pin.H"
#include <iostream>
#include <map>
#include "NtStructures.h"
#include "InstrumentationStrategy.h"

struct InstNtOpenKeyArgs {
    ADDRINT KeyHandle;
    ADDRINT DesiredAccess;
    ADDRINT ObjectAttributes;
};

class InstNtOpenKey : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstNtOpenKeyArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes);
};

#endif // INST_NT_OPEN_KEY_H
