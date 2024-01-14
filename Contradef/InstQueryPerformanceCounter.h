#pragma once
#ifndef INST_QUERY_PERFORMANCE_COUNTER_H
#define INST_QUERY_PERFORMANCE_COUNTER_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstQueryPerformanceCounterArgs {
    ADDRINT lpPerformanceCount;
};

class InstQueryPerformanceCounter : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstQueryPerformanceCounterArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT lpPerformanceCount);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpPerformanceCount);
};

#endif // INST_QUERY_PERFORMANCE_COUNTER_H
