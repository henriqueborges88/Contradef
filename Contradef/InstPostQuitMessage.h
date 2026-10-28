#pragma once
#ifndef INST_POST_QUIT_MESSAGE_H
#define INST_POST_QUIT_MESSAGE_H

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

// VOID PostQuitMessage(int nExitCode);
struct PostQuitMessageArgs {
    ADDRINT nExitCode; // int
};

class InstPostQuitMessage : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData*, void*) {}
    static VOID HandleTraceEvent(const EventData*, void*) {}

private:
    static CallContextMap callContextMap;
    static UINT32 imgCallId;
    static CallSequenceNumber fcnCallId;
    static Notifier* globalNotifierPtr;

    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT returnAddress,
        ADDRINT nExitCode);

    // Função é VOID -> no AFTER não há valor de retorno
    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT* returnAddress,
        ADDRINT nExitCode);
};

#endif // INST_POST_QUIT_MESSAGE_H
