#pragma once
#ifndef INST_VIRTUAL_PROTECT_H
#define INST_VIRTUAL_PROTECT_H

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

struct InstVirtualProtectArgs {
    ADDRINT lpAddress;
    ADDRINT dwSize;
    ADDRINT flNewProtect;
    ADDRINT lpflOldProtect;
};

class InstVirtualProtect : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);

private:
    static std::map<CallContextKey, CallContext*> callContextMap;
    static UINT32 imgCallId;
    static UINT32 fcnCallId;
    static Notifier* globalNotifierPtr;
    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT returnAddress,
        ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress,
        ADDRINT lpAddress, ADDRINT dwSize, ADDRINT flNewProtect, ADDRINT lpflOldProtect);
};

#endif // INST_VIRTUAL_PROTECT_H
