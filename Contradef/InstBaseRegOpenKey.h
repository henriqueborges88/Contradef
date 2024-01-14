#pragma once
#ifndef INST_BASE_REG_OPEN_KEY_H
#define INST_BASE_REG_OPEN_KEY_H

#include "pin.H"
#include <iostream>
#include <map>
#include "InstrumentationStrategy.h"


struct InstBaseRegOpenKeyArgs {
    ADDRINT hKey;
    ADDRINT lpSubKey;
    ADDRINT phkResult;
};

class InstBaseRegOpenKey : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstBaseRegOpenKeyArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT hKey, ADDRINT lpSubKey, ADDRINT phkResult);
};

#endif // INST_BASE_REG_OPEN_KEY_H
