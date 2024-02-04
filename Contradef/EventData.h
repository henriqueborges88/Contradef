#pragma once
#ifndef EVENTDATA_H
#define EVENTDATA_H

#include <string>

// Classe base para eventos
class EventData {
public:
    enum EventType { Instruction, CallTrace };

    EventType type; // Identificador do tipo de evento

    // Construtor protegido para forçar o uso apenas em subclasses
protected:
    explicit EventData(EventType type) : type(type) {}

public:
    virtual ~EventData() = default; // Destrutor virtual para herança
};

struct InstuctionInformation {
    std::queue<InstuctionInformation>* instructionTrace;
    ADDRINT address;
    std::string disassembledInstruction;
};

// Evento de instrução
class InstructionEventData : public EventData {
public:
    InstuctionInformation instuctionInformation;

    InstructionEventData(const InstuctionInformation& iInformation)
        : EventData(Instruction), instuctionInformation(iInformation) {}
};

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

// Evento de trace
class TraceEventData : public EventData {
public:
    CallTraceInformation callTraceInfo;

    TraceEventData(const CallTraceInformation& cTraceInfo)
        : EventData(CallTrace), callTraceInfo(cTraceInfo) {}
};

#endif // EVENTDATA_H
