#pragma once
#ifndef INST_BLOCK_INPUT_H
#define INST_BLOCK_INPUT_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstBlockInputArgs {
    ADDRINT fBlockIt;
};

class InstBlockInput : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstBlockInputArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT fBlockIt);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT fBlockIt);
};

#endif // INST_BLOCK_INPUT_H
