#pragma once
#ifndef INST_REG_OPEN_KEY_EX_A_H
#define INST_REG_OPEN_KEY_EX_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"

struct InstRegOpenKeyExAArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT ulOptions;
    ADDRINT samDesired;
    ADDRINT phkResult;
};

class InstRegOpenKeyExA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstRegOpenKeyExAArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT ulOptions, ADDRINT samDesired, ADDRINT phkResult);
};

#endif // INST_REG_OPEN_KEY_EX_A_H
