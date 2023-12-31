#pragma once
#ifndef INST_NT_QUERY_INFORMATION_PROCESS_H
#define INST_NT_QUERY_INFORMATION_PROCESS_H

#include "pin.H"
#include <iostream>
#include <map>
#include <string>

struct NtQueryInformationProcessArgs {
    ADDRINT ProcessHandle;
    ADDRINT ProcessInformationClass;
    ADDRINT ProcessInformation;
    ADDRINT ProcessInformationLength;
    ADDRINT ReturnLength;
};

class InstNtQueryInformationProcess {
public:
    static VOID InstrumentFunction(RTN rtn);

private:
    static std::map<ADDRINT, NtQueryInformationProcessArgs> argsMap;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);
};

#endif // INST_NT_QUERY_INFORMATION_PROCESS_H
