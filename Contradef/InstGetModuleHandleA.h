#pragma once
#ifndef INST_GET_MODULE_HANDLE_A_H
#define INST_GET_MODULE_HANDLE_A_H

#include "pin.H"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <deque>
#include <queue>
#include "InstrumentationStrategy.h"
#include "utils.h"
#include  "CallContextKey.h"
#include "Notifier.h"
#include "Observer.h"

struct GetModuleHandleAArgs {
    ADDRINT lpModuleName;
};

struct InstuctionInfo {
    ADDRINT address;
    std::string disassembledInstruction;
};

struct CallContext {
    ADDRINT rtnAddress;
    std::stringstream stringStream;
    GetModuleHandleAArgs getModuleHandleAArgs;
    UINT32 maxInst = 0;
    UINT32 countInstructionsAfterCall = 0;
    UINT32 countFunctionsCalledAfterCall = 0;
    std::vector<InstuctionInformation> instructionsAfterCall;
    std::vector<CallRtnInformation> functionsCalledAfterCall;
    UINT32 callId; // Identificador único para cada chamada
    THREADID threadId; // Identificador da thread
    // Observadores dinâmicos específicos deste contexto
    Observer* instructionObserver = nullptr;
    Observer* traceObserver = nullptr;

    // Construtor para inicializar o contexto com IDs
    CallContext(UINT32 cid, THREADID tid, ADDRINT rtnAddr, GetModuleHandleAArgs args) : callId(cid), threadId(tid), rtnAddress(rtnAddr), getModuleHandleAArgs(args) {}
};

class InstGetModuleHandleA : public InstrumentationStrategy {
public:
    static VOID InstrumentFunction(RTN rtn, std::ofstream& out, Notifier& globalNotifier);
    static VOID HandleInstructionEvent(const EventData* data, void* context);
    static VOID HandleTraceEvent(const EventData* data, void* context);

private:
    static std::map<CallContextKey, CallContext*> callContextMap;
    static UINT32 imgCallId;
    static UINT32 fcnCallId;
    static std::ofstream* outStream;
    static Notifier* globalNotifierPtr;
    static VOID CheckPrintConditions(CallContext* callContext);
    static VOID CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT lpModuleName);
    static VOID CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT lpModuleName);
};

#endif // INST_GET_MODULE_HANDLE_A_H