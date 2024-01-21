#pragma once
#ifndef CALL_CONTEXT_KEY_H
#define CALL_CONTEXT_KEY_H

#include "pin.H"

struct CallContextKey {
    ADDRINT callId;
    THREADID threadId;
    bool operator<(const CallContextKey& other) const {
        if (callId < other.callId) return true;
        if (callId > other.callId) return false;
        return threadId < other.threadId;
    }
};

#endif // CALL_CONTEXT_KEY_H
