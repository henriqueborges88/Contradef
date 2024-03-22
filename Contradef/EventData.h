#pragma once
#ifndef EVENTDATA_H
#define EVENTDATA_H

#include <string>
#include <queue>

// Classe base para eventos
class EventData {
public:
    enum EventType { Instruction, CallTrace, ExecutionCompleted };

    EventType type; // Identificador do tipo de evento

    // Construtor protegido para forçar o uso apenas em subclasses
protected:
    explicit EventData(EventType type) : type(type) {}

public:
    virtual ~EventData() = default; // Destrutor virtual para herança
};



// Evento de instrução
struct InstructionInformation {
    std::queue<InstructionInformation>* instructionTrace;
    ADDRINT address;
    std::string disassembledInstruction;
};
class InstructionEventData : public EventData {
public:
    InstructionInformation instuctionInformation;

    InstructionEventData(const InstructionInformation& iInformation)
        : EventData(Instruction), instuctionInformation(iInformation) {}
};

// Evento de trace
struct CallRtnInformation {
    ADDRINT address;
    std::string functionName;
    std::string imgName;
    std::string rtnCallerName;
};

struct CallTraceInformation {
    std::map<THREADID, std::deque<CallRtnInformation>>* threadCallTraces;
    CallRtnInformation callRtnInformation;
};

class TraceEventData : public EventData {
public:
    CallTraceInformation callTraceInfo;

    TraceEventData(const CallTraceInformation& cTraceInfo)
        : EventData(CallTrace), callTraceInfo(cTraceInfo) {}
};

// Evento de finalização de execução
struct ExecutionInformation {
    std::string outputText;
};
class ExecutionEventData : public EventData {
public:
    ExecutionInformation executionInformation;

    ExecutionEventData(const ExecutionInformation& iInformation)
        : EventData(ExecutionCompleted), executionInformation(iInformation) {}
};

#endif // EVENTDATA_H
