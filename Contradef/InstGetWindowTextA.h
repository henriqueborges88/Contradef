#pragma once
#ifndef INST_GET_WINDOW_TEXT_A_H
#define INST_GET_WINDOW_TEXT_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"
#include  "utils.h"

struct InstGetWindowTextAArgs {
    ADDRINT hWnd;
    ADDRINT lpString;
    ADDRINT nMaxCount;
};

class InstGetWindowTextA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstGetWindowTextAArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT hWnd, ADDRINT lpString, ADDRINT nMaxCount);
};

#endif // INST_GET_WINDOW_TEXT_A_H
