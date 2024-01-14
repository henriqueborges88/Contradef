#pragma once
#ifndef INST_GET_FOREGROUND_WINDOW_H
#define INST_GET_FOREGROUND_WINDOW_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

class InstGetForegroundWindow : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    typedef UINT64 HWND;   // HWND é normalmente um ponteiro ou um valor integral
    static VOID CallbackBefore(ADDRINT instrPtr);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr);
};

#endif // INST_GET_FOREGROUND_WINDOW_H
