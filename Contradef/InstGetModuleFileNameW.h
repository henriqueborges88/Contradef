#pragma once
#ifndef INST_GET_MODULE_FILE_NAME_W_H
#define INST_GET_MODULE_FILE_NAME_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"

struct InstGetModuleFileNameWArgs {
    ADDRINT hModule;
    ADDRINT lpFilename;
    ADDRINT nSize;
};

class InstGetModuleFileNameW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstGetModuleFileNameWArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize);
};

#endif // INST_GET_MODULE_FILE_NAME_W_H
