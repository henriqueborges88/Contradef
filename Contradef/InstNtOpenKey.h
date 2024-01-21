#pragma once
#ifndef INST_NT_OPEN_KEY_H
#define INST_NT_OPEN_KEY_H

#include "pin.H"
#include <iostream>
#include <map>
#include "NtStructures.h"
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

struct InstNtOpenKeyArgs {
    ADDRINT KeyHandle;
    ADDRINT DesiredAccess;
    ADDRINT ObjectAttributes;
};

class InstNtOpenKey : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstNtOpenKeyArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT KeyHandle, ADDRINT DesiredAccess, ADDRINT ObjectAttributes);
};

#endif // INST_NT_OPEN_KEY_H
