#pragma once
#ifndef INST_REG_OPEN_KEY_A_H
#define INST_REG_OPEN_KEY_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstRegOpenKeyAArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstRegOpenKeyA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstRegOpenKeyAArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_A_H
