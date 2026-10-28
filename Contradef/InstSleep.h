#pragma once
#ifndef INST_SLEEP_H
#define INST_SLEEP_H

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

// -------- argumentos da função --------
struct SleepArgs {
    ADDRINT dwMilliseconds;   // DWORD
};

class InstSleep : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData*, void*) {}
    static VOID HandleTraceEvent(const EventData*, void*) {}

private:
    static CallContextMap callContextMap;
    static UINT32 imgCallId;
    static CallSequenceNumber fcnCallId;
    static Notifier* globalNotifierPtr;

    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT returnAddress,
        ADDRINT dwMilliseconds);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT* returnAddress,
        ADDRINT dwMilliseconds);
};

#endif // INST_SLEEP_H
