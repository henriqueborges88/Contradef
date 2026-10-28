#pragma once
#ifndef INST_GET_MESSAGE_A_H
#define INST_GET_MESSAGE_A_H

#include "pin.H"
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include "utils.h"
#include "CallContext.h"
#include "Notifier.h"
#include "Observer.h"
#include "Instrumentation.h"
#include "InstrumentationStrategy.h"

// BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);

struct GetMessageAArgs {
    ADDRINT lpMsg;          // LPMSG
    ADDRINT hWnd;           // HWND
    ADDRINT wMsgFilterMin;  // UINT
    ADDRINT wMsgFilterMax;  // UINT
};

class InstGetMessageA : public InstrumentationStrategy {
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
        ADDRINT lpMsg, ADDRINT hWnd, ADDRINT wMsgFilterMin, ADDRINT wMsgFilterMax);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT retValue,
        ADDRINT* returnAddress,
        ADDRINT lpMsg, ADDRINT hWnd, ADDRINT wMsgFilterMin, ADDRINT wMsgFilterMax);
};

#endif // INST_GET_MESSAGE_A_H
