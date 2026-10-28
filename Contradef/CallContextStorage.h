#pragma once

#include "pin.H"
#include <map>
#include <vector>

// Initialized once before application callbacks can execute.
inline PIN_MUTEX& CallContextMutex()
{
    static PIN_MUTEX mutex;
    return mutex;
}

inline void InitCallContextStorage()
{
    const BOOL initialized = PIN_MutexInit(&CallContextMutex());
    ASSERTX(initialized);
}

class CallContextLockGuard
{
public:
    CallContextLockGuard() { PIN_MutexLock(&CallContextMutex()); }
    ~CallContextLockGuard() { PIN_MutexUnlock(&CallContextMutex()); }
    CallContextLockGuard(const CallContextLockGuard&) = delete;
    CallContextLockGuard& operator=(const CallContextLockGuard&) = delete;
};

// Contexts are paired by routine and thread, with a stack for reentrant calls.
// CallContext and CallContextKey are declared by CallContext.h before this file.
using CallContextMap = std::map<CallContextKey, std::vector<CallContext*>>;

inline void SaveCallContext(CallContextMap& contexts, const CallContextKey& key,
                            CallContext* context)
{
    CallContextLockGuard guard;
    contexts[key].push_back(context);
}

inline CallContext* TakeCallContext(CallContextMap& contexts, const CallContextKey& key)
{
    CallContextLockGuard guard;
    auto it = contexts.find(key);
    if (it == contexts.end()) return nullptr;

    CallContext* context = it->second.back();
    it->second.pop_back();
    if (it->second.empty()) contexts.erase(it);
    return context;
}

// Keep the existing completion counter for log IDs without racing its reads.
class CallSequenceNumber
{
    UINT32 value;

public:
    CallSequenceNumber(UINT32 initial = 0) : value(initial) {}

    operator UINT32() const
    {
        CallContextLockGuard guard;
        return value;
    }

    UINT32 operator++(int)
    {
        CallContextLockGuard guard;
        return value++;
    }
};
