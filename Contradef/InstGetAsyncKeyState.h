#pragma once
#ifndef INST_GET_ASYNC_KEY_STATE_H
#define INST_GET_ASYNC_KEY_STATE_H

#include "pin.H"
#include <iostream>
#include <map>

struct InstGetAsyncKeyStateArgs {
    ADDRINT vKey;
};

class InstGetAsyncKeyState {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstGetAsyncKeyStateArgs> argsMap;

    typedef short SHORT;
    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT vKey);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT vKey);
};

#endif // INST_GET_ASYNC_KEY_STATE_H
