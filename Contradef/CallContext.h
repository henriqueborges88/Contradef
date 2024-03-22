#pragma once
#ifndef CALL_CONTEXT_H
#define CALL_CONTEXT_H

#include "pin.H"
#include "EventData.h"
#include "Observer.h"
#include "Notifier.h"

struct CallContextKey {
    UINT32 callId;
    THREADID threadId;
    bool operator<(const CallContextKey& other) const {
        if (callId < other.callId) return true;
        if (callId > other.callId) return false;
        return threadId < other.threadId;
    }
};

struct CallContext {
    BOOL defaultPrintReady = false;
    ADDRINT rtnAddress;
    std::stringstream stringStream;
    VOID* functionArgs;
    UINT32 maxInst = 0;
    UINT32 countInstructionsAfterCall = 0;
    UINT32 countFunctionsCalledAfterCall = 0;
    std::vector<InstructionInformation> instructionsAfterCall;
    std::vector<CallRtnInformation> functionsCalledAfterCall;
    UINT32 callId; // Identificador único para cada chamada
    THREADID threadId; // Identificador da thread
    // Observadores dinâmicos específicos deste contexto
    Observer* instructionObserver = nullptr;
    Observer* traceObserver = nullptr;

    // Construtor para inicializar o contexto com IDs
    CallContext(UINT32 cid, THREADID tid, ADDRINT rtnAddr, VOID* args) : callId(cid), threadId(tid), rtnAddress(rtnAddr), functionArgs(args) {}
};

VOID CheckPrintConditionsCC(CallContext* callContext, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr);
VOID HandleInstructionEventCC(const EventData* data, void* context, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr);
VOID HandleTraceEventCC(const EventData* data, void* context, std::map<CallContextKey, CallContext*>* callContextMap, Notifier* notifierPtr);

#endif // CALL_CONTEXT_H
