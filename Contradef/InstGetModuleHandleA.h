#pragma once
#ifndef INST_GET_MODULE_HANDLE_A_H
#define INST_GET_MODULE_HANDLE_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct GetModuleHandleAArgs {
    ADDRINT lpModuleName;
};

class InstGetModuleHandleA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, GetModuleHandleAArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpModuleName);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpModuleName);
};

#endif // INST_GET_MODULE_HANDLE_A_H