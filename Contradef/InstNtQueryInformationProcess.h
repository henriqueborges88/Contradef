#pragma once
#ifndef INST_NT_QUERY_INFORMATION_PROCESS_H
#define INST_NT_QUERY_INFORMATION_PROCESS_H

#include "pin.H"
#include <map>
#include <string>
#include <fstream>
#include <set>
#include <vector>
#include "utils.h"
#include "NtStructures.h"
#include "InstrumentationStrategy.h"
#include  "CallContextKey.h"

struct NtQueryInformationProcessArgs {
    ADDRINT ProcessHandle;
    ADDRINT ProcessInformationClass;
    ADDRINT ProcessInformation;
    ADDRINT ProcessInformationLength;
    ADDRINT ReturnLength;
};

class InstNtQueryInformationProcess : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out);

private:

    static std::map<CallContextKey, NtQueryInformationProcessArgs> argsMap;
    static ADDRINT TARGET_MEMORY_ADDRESS;
    static ADDRINT nextCallId;
    static std::ofstream* outStream;
    static LONG retVal;

    static DWORD InstNtQueryInformationProcess::NtGlobalFlag;

    static VOID CallbackBefore(THREADID tid, ADDRINT callId, ADDRINT instrPtr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(THREADID tid, ADDRINT callId, ADDRINT instrPtr, ADDRINT* retValAddr, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength);

    static VOID ReadMemoryValue(CONTEXT* ctxt, INS ins, ADDRINT instAddress, ADDRINT memoryAddress, UINT32 readSize);
    static bool IsRegisterPermitted(const REG& regName);
    static VOID RegContentAnalysisRoutine(CONTEXT* ctxt, REG regToInspect);
    static VOID Instruction(INS ins, VOID* v);
};

#endif // INST_NT_QUERY_INFORMATION_PROCESS_H
