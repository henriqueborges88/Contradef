#pragma once
#ifndef INST_GET_MODULE_HANDLE_A_H
#define INST_GET_MODULE_HANDLE_A_H

#include "pin.H"
#include <iostream>
#include <map>
#include <string>

struct GetModuleHandleAArgs {
    ADDRINT lpModuleName;
};

class InstGetModuleHandleA {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, GetModuleHandleAArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT lpModuleName);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpModuleName);
};

#endif // INST_GET_MODULE_HANDLE_A_H
