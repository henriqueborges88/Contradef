#pragma once
#ifndef INST_ZW_QUERY_VIRTUAL_MEMORY_H
#define INST_ZW_QUERY_VIRTUAL_MEMORY_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <deque>
#include <queue>
#include "utils.h"
#include "CallContext.h"
#include "Notifier.h"
#include "Observer.h"
#include "Instrumentation.h"
#include "InstrumentationStrategy.h"

struct InstZwQueryVirtualMemoryArgs {
    ADDRINT ProcessHandle;
    ADDRINT BaseAddress;
    ADDRINT MemoryInformationClass;
    ADDRINT MemoryInformation;
    ADDRINT MemoryInformationLength;
    ADDRINT ReturnLength;
};

class InstZwQueryVirtualMemory : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData* data, void* context);
    static VOID HandleTraceEvent(const EventData* data, void* context);

private:
    static std::map<CallContextKey, CallContext*> callContextMap;
    static UINT32 imgCallId;
    static UINT32 fcnCallId;
    static Notifier* globalNotifierPtr;
    static VOID CheckPrintConditions(CallContext* callContext);
    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress,
        ADDRINT ProcessHandle, ADDRINT BaseAddress, ADDRINT MemoryInformationClass, ADDRINT MemoryInformation,
        ADDRINT MemoryInformationLength, ADDRINT ReturnLength);
    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
        ADDRINT ProcessHandle, ADDRINT BaseAddress, ADDRINT MemoryInformationClass, ADDRINT MemoryInformation,
        ADDRINT MemoryInformationLength, ADDRINT ReturnLength);
};

#endif // INST_ZW_QUERY_VIRTUAL_MEMORY_H
