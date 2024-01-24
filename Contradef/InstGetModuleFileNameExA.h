#pragma once
#ifndef INST_GET_MODULE_FILE_NAME_EX_A_H
#define INST_GET_MODULE_FILE_NAME_EX_A_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"
#include "utils.h"

struct InstGetModuleFileNameExAArgs {
    ADDRINT hProcess;
    ADDRINT hModule;
    ADDRINT lpFilename;
    ADDRINT nSize;
};

class InstGetModuleFileNameExA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstGetModuleFileNameExAArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hProcess, ADDRINT hModule, ADDRINT lpFilename, ADDRINT nSize);
};

#endif // INST_GET_MODULE_FILE_NAME_EX_A_H
