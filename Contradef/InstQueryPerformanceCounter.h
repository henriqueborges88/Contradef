#pragma once
#ifndef INST_QUERY_PERFORMANCE_COUNTER_H
#define INST_QUERY_PERFORMANCE_COUNTER_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContext.h"
#include "utils.h"

struct InstQueryPerformanceCounterArgs {
    ADDRINT lpPerformanceCount;
};

class InstQueryPerformanceCounter : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstQueryPerformanceCounterArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpPerformanceCount);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpPerformanceCount);
};

#endif // INST_QUERY_PERFORMANCE_COUNTER_H
