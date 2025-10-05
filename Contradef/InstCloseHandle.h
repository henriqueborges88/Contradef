#pragma once
#ifndef INST_CLOSE_HANDLE_H
#define INST_CLOSE_HANDLE_H

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
struct CloseHandleArgs {
    ADDRINT hObject;   // HANDLE
};

class InstCloseHandle : public InstrumentationStrategy {
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
        ADDRINT hObject);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT inst, ADDRINT rtn, CONTEXT*,
        ADDRINT* returnAddress,
        ADDRINT retValue,
        ADDRINT hObject);
};

#endif // INST_CLOSE_HANDLE_H
