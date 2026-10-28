#pragma once
#ifndef INST_GET_MODULE_HANDLE_EX_W_H
#define INST_GET_MODULE_HANDLE_EX_W_H

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

// Assinatura:
// BOOL GetModuleHandleExW(
//   DWORD    dwFlags,
//   LPCWSTR  lpModuleName,
//   HMODULE* phModule
// );

struct GetModuleHandleExWArgs {
    ADDRINT dwFlags;       // DWORD
    ADDRINT lpModuleName;  // LPCWSTR
    ADDRINT phModule;      // HMODULE*
};

class InstGetModuleHandleExW : public InstrumentationStrategy {
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
        ADDRINT dwFlags, ADDRINT lpModuleName, ADDRINT phModule);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT* retValueRef,
        ADDRINT* returnAddress,
        ADDRINT dwFlags, ADDRINT lpModuleName, ADDRINT phModule);
};

#endif // INST_GET_MODULE_HANDLE_EX_W_H
