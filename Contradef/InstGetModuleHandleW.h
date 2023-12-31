#pragma once
#ifndef INST_GET_MODULE_HANDLE_W_H
#define INST_GET_MODULE_HANDLE_W_H

#include "pin.H"
#include <iostream>
#include <map>
#include <string>

struct InstGetModuleHandleWArgs {
    ADDRINT lpModuleName;
};

class InstGetModuleHandleW {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, InstGetModuleHandleWArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT lpModuleName);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT lpModuleName);
};

#endif // INST_GET_MODULE_HANDLE_W_H
