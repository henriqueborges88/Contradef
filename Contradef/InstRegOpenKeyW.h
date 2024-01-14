#pragma once
#ifndef INST_REG_OPEN_KEY_W_H
#define INST_REG_OPEN_KEY_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstRegOpenKeyWArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstRegOpenKeyW : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstRegOpenKeyWArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_W_H
