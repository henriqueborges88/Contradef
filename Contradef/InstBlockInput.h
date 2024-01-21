#pragma once
#ifndef INST_BLOCK_INPUT_H
#define INST_BLOCK_INPUT_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

struct InstBlockInputArgs {
    ADDRINT fBlockIt;
};

class InstBlockInput : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<CallContextKey, InstBlockInputArgs> argsMap;
    static ADDRINT nextCallId;
    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT fBlockIt);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT* retValAddr, ADDRINT fBlockIt);
};

#endif // INST_BLOCK_INPUT_H
