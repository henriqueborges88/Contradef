#pragma once
#ifndef INSTRUMENTATION_CONTEXT_H
#define INSTRUMENTATION_CONTEXT_H

#include "InstrumentationStrategy.h"
#include <map>
#include <string>
#include <functional>

typedef void (*RTNFunction)(RTN);

class InstrumentationContext {
private:
    std::map<std::string, RTNFunction> strategyMap;

public:
    InstrumentationContext();
    ~InstrumentationContext();
    void ExecuteStrategy(const std::string& name, RTN rtn);
    bool HasStrategy(const std::string& name) const;
    void ExecuteAllStrategies(IMG img);
};

#endif // INSTRUMENTATION_CONTEXT_H