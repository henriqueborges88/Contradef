#pragma once
#ifndef INST_IS_DEBUGGER_PRESENT_H
#define INST_IS_DEBUGGER_PRESENT_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

class InstIsDebuggerPresent : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static VOID CallbackBefore(ADDRINT instrPtr);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr);
};

#endif // INST_IS_DEBUGGER_PRESENT_H
