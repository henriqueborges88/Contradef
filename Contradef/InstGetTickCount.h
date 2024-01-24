#pragma once
#ifndef INST_GET_TICK_COUNT_H
#define INST_GET_TICK_COUNT_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include "InstrumentationStrategy.h"

class InstGetTickCount : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:
    typedef unsigned long DWORD;
    static std::ofstream* outStream;
    static VOID CallbackBefore(ADDRINT instrPtr);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr);
};

#endif // INST_GET_TICK_COUNT_H
