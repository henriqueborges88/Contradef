#pragma once
#ifndef INST_CREATE_WINDOW_EX_A_H
#define INST_CREATE_WINDOW_EX_A_H

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
// HWND CreateWindowExA(
//   DWORD   dwExStyle,
//   LPCSTR  lpClassName,
//   LPCSTR  lpWindowName,
//   DWORD   dwStyle,
//   int     X,
//   int     Y,
//   int     nWidth,
//   int     nHeight,
//   HWND    hWndParent,
//   HMENU   hMenu,
//   HINSTANCE hInstance,
//   LPVOID  lpParam );

struct CreateWindowExAArgs {
    ADDRINT dwExStyle;
    ADDRINT lpClassName;
    ADDRINT lpWindowName;
    ADDRINT dwStyle;
    ADDRINT X;
    ADDRINT Y;
    ADDRINT nWidth;
    ADDRINT nHeight;
    ADDRINT hWndParent;
    ADDRINT hMenu;
    ADDRINT hInstance;
    ADDRINT lpParam;
};

class InstCreateWindowExA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData*, void*) {}
    static VOID HandleTraceEvent(const EventData*, void*) {}

private:
    static std::map<CallContextKey, CallContext*> callContextMap;
    static UINT32 imgCallId;
    static UINT32 fcnCallId;
    static Notifier* globalNotifierPtr;

    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT returnAddress,
        ADDRINT dwExStyle, ADDRINT lpClassName, ADDRINT lpWindowName, ADDRINT dwStyle,
        ADDRINT X, ADDRINT Y, ADDRINT nWidth, ADDRINT nHeight,
        ADDRINT hWndParent, ADDRINT hMenu, ADDRINT hInstance, ADDRINT lpParam);

    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT rtn, CONTEXT* ctx,
        ADDRINT retValue,
        ADDRINT* returnAddress,
        ADDRINT dwExStyle, ADDRINT lpClassName, ADDRINT lpWindowName, ADDRINT dwStyle,
        ADDRINT X, ADDRINT Y, ADDRINT nWidth, ADDRINT nHeight,
        ADDRINT hWndParent, ADDRINT hMenu, ADDRINT hInstance, ADDRINT lpParam);
};

#endif // INST_CREATE_WINDOW_EX_A_H
