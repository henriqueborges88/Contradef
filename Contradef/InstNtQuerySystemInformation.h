#pragma once
#ifndef INST_NT_QUERY_SYSTEM_INFORMATION_H
#define INST_NT_QUERY_SYSTEM_INFORMATION_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

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
    static std::map<CallContextKey, InstNtQuerySystemInformationArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT SystemInformationClass, ADDRINT SystemInformation, ADDRINT SystemInformationLength, ADDRINT ReturnLength);
};

#endif // INST_NT_QUERY_SYSTEM_INFORMATION_H
