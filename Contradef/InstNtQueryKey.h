#pragma once
#ifndef INST_NT_QUERY_KEY_H
#define INST_NT_QUERY_KEY_H

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

// ----------  argumentos ----------
struct NtQueryKeyArgs {
    ADDRINT KeyHandle;           // HANDLE
    ADDRINT KeyInformationClass; // KEY_INFORMATION_CLASS
    ADDRINT KeyInformation;      // PVOID
    ADDRINT Length;              // ULONG
    ADDRINT ResultLength;        // PULONG
};

class InstNtQueryKey : public InstrumentationStrategy {
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
        ADDRINT returnAddress,
        ADDRINT KeyHandle, ADDRINT KeyInformationClass,
        ADDRINT KeyInformation, ADDRINT Length, ADDRINT ResultLength);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT* returnAddress,
        ADDRINT retValue,
        ADDRINT KeyHandle, ADDRINT KeyInformationClass,
        ADDRINT KeyInformation, ADDRINT Length, ADDRINT ResultLength);
};

#endif // INST_NT_QUERY_KEY_H
