#pragma once
#ifndef INST_OUTPUT_DEBUG_STRING_H
#define INST_OUTPUT_DEBUG_STRING_H

#include "pin.H"
#include <iostream>
#include <map>
#include <string>
#include "InstrumentationStrategy.h"

struct InstOutputDebugStringArgs {
    ADDRINT lpOutputString;
};

class InstOutputDebugString : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstOutputDebugStringArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT lpOutputString);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT lpOutputString);
};

#endif // INST_OUTPUT_DEBUG_STRING_H
