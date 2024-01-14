#pragma once
#ifndef INST_NT_QUERY_SYSTEM_INFORMATION_H
#define INST_NT_QUERY_SYSTEM_INFORMATION_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstNtQuerySystemInformationArgs {
    ADDRINT SystemInformationClass;
    ADDRINT SystemInformation;
    ADDRINT SystemInformationLength;
    ADDRINT ReturnLength;
};

class InstNtQuerySystemInformation : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstNtQuerySystemInformationArgs> argsMap;

    typedef long LONG;
    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength);
};

#endif // INST_NT_QUERY_SYSTEM_INFORMATION_H
