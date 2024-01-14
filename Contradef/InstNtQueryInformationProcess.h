#pragma once
#ifndef INST_NT_QUERY_INFORMATION_PROCESS_H
#define INST_NT_QUERY_INFORMATION_PROCESS_H

#include "pin.H"
#include <map>
#include <string>
#include <set>
#include <vector>
#include "NtStructures.h"
#include "InstrumentationStrategy.h"

struct NtQueryInformationProcessArgs {
    ADDRINT ProcessHandle;
    ADDRINT ProcessInformationClass;
    ADDRINT ProcessInformation;
    ADDRINT ProcessInformationLength;
    ADDRINT ReturnLength;
};

class InstNtQueryInformationProcess : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn);

private:

    static std::map<ADDRINT, NtQueryInformationProcessArgs> argsMap;
    static ADDRINT TARGET_MEMORY_ADDRESS;
    static DWORD InstNtQueryInformationProcess::NtGlobalFlag;

    static VOID CallbackBefore(ADDRINT instrPtr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);

    static VOID ReadMemoryValue(CONTEXT* ctxt, INS ins, ADDRINT instAddress, ADDRINT memoryAddress, UINT32 readSize);
    static bool IsRegisterPermitted(const REG& regName);
    static VOID RegContentAnalysisRoutine(CONTEXT* ctxt, REG regToInspect);
    static VOID Instruction(INS ins, VOID* v);
};

#endif // INST_NT_QUERY_INFORMATION_PROCESS_H
