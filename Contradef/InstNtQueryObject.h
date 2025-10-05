#pragma once
#ifndef INST_NT_QUERY_OBJECT_H
#define INST_NT_QUERY_OBJECT_H

#include "pin.H"
#include <iostream>
#include <sstream>
#include <map>
#include "utils.h"
#include "CallContext.h"
#include "Notifier.h"
#include "Observer.h"
#include "Instrumentation.h"
#include "InstrumentationStrategy.h"

// ---------- argumentos ----------
struct NtQueryObjectArgs {
    ADDRINT Handle;                // HANDLE
    ADDRINT ObjectInformationClass;// OBJECT_INFORMATION_CLASS
    ADDRINT ObjectInformation;     // PVOID
    ADDRINT ObjectInformationLength;// ULONG
    ADDRINT ReturnLength;          // PULONG
};

class InstNtQueryObject : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData*, void*) {}
    static VOID HandleTraceEvent(const EventData*, void*) {}

private:
    static std::map<CallContextKey, CallContext*> callContextMap;
    static UINT32 imgCallId;
    static UINT32 fcnCallId;
    static Notifier* globalNotifierPtr;

    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT retIP,
        ADDRINT Handle, ADDRINT ObjectInformationClass,
        ADDRINT ObjectInformation, ADDRINT ObjectInformationLength,
        ADDRINT ReturnLength);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT* retIP,
        ADDRINT retValue,
        ADDRINT Handle, ADDRINT ObjectInformationClass,
        ADDRINT ObjectInformation, ADDRINT ObjectInformationLength,
        ADDRINT ReturnLength);
};

#endif // INST_NT_QUERY_OBJECT_H
