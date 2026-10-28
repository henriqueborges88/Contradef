#pragma once
#ifndef INST_KI_USER_EXCEPTION_DISPATCHER_H
#define INST_KI_USER_EXCEPTION_DISPATCHER_H

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

// Parâmetros de KiUserExceptionDispatcher
// VOID KiUserExceptionDispatcher(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT Context)
struct KiUserExceptionDispatcherArgs {
    ADDRINT pExceptionRecord;  // PEXCEPTION_RECORD
    ADDRINT pContext;          // PCONTEXT
};

struct PatchInfo {
    bool   inspected{ false };
    bool   patched{ false };
    WindowsAPI::DWORD  oldCode{ 0 };
    WindowsAPI::DWORD  oldFlags{ 0 };
    WindowsAPI::DWORD  oldNum{ 0 };
    WindowsAPI::DWORD  newCode{ 0 };
    WindowsAPI::DWORD  newFlags{ 0 };
    WindowsAPI::DWORD  newNum{ 0 };
};

class InstKiUserExceptionDispatcher : public InstrumentationStrategy {
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
        ADDRINT pExceptionRecord,
        ADDRINT pContext);

    // NOTA: função é VOID -> não passamos IARG_FUNCRET_EXITPOINT_VALUE
    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT* returnAddress,  // obtida via REG_STACK_PTR
        ADDRINT pExceptionRecord,
        ADDRINT pContext);
};

#endif // INST_KI_USER_EXCEPTION_DISPATCHER_H
