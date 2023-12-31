#pragma once
#ifndef INST_GET_TICK_COUNT_H
#define INST_GET_TICK_COUNT_H

#include "pin.H"
#include <iostream>

class InstGetTickCount {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    typedef unsigned long DWORD;

    static VOID CallbackBefore(ADDRINT instrPtr);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr);
};

#endif // INST_GET_TICK_COUNT_H
