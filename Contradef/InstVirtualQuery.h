#pragma once
#ifndef INST_VIRTUAL_QUERY_H
#define INST_VIRTUAL_QUERY_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include "CallContextKey.h"
#include "utils.h"
#include "NtStructures.h"

struct InstVirtualQueryArgs {
    ADDRINT lpAddress;
    ADDRINT lpBuffer;
    ADDRINT dwLength;
};

class InstVirtualQuery : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstVirtualQueryArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static unsigned long InstVirtualQuery::show_module(MEMORY_BASIC_INFORMATION info);
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpAddress, ADDRINT lpBuffer, ADDRINT dwLength);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpAddress, ADDRINT lpBuffer, INT dwLength);
};

#endif // INST_VIRTUAL_QUERY_H
