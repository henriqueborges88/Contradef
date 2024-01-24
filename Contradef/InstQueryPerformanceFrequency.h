#pragma once
#ifndef INST_QUERY_PERFORMANCE_FREQUENCY_H
#define INST_QUERY_PERFORMANCE_FREQUENCY_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

struct InstQueryPerformanceFrequencyArgs {
    ADDRINT lpFrequency;
};

class InstQueryPerformanceFrequency : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    static std::map<CallContextKey, InstQueryPerformanceFrequencyArgs> argsMap;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT lpFrequency);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT lpFrequency);
};

#endif // INST_QUERY_PERFORMANCE_FREQUENCY_H
