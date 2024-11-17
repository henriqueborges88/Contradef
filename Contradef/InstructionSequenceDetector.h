#pragma once
#ifndef INSTRUCTION_SEQUENCE_DETECTOR_H
#define INSTRUCTION_SEQUENCE_DETECTOR_H

#include "pin.H"
#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <unordered_map>

void NormalizeInstructionText(std::string& instr);

// Forward declaration
struct SequenceInstructions;

// Definição do tipo de callback usando a declaração antecipada de SequenceInstruction
using SeqMatchCallbackFunc = VOID(*)(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence);

struct InstrumentedInstruction {
    ADDRINT instAddr;
    std::string instructionStr;
    std::string normalizedInstructionStr;
    int size;

    InstrumentedInstruction(std::string instructionStr)
        : instAddr(0), instructionStr(instructionStr), normalizedInstructionStr(instructionStr), size(0)
    {
        NormalizeInstructionText(normalizedInstructionStr);
        size = strlen(normalizedInstructionStr.c_str());
    }
};

struct InstructionToMatch {
    ADDRINT matchAddr;
    std::string instructionStr;
    std::string normalizedInstructionStr;
    bool isAnyInst;
    bool hasWilcard;
    int size;

    // Construtor para inicializar o contexto com IDs
    InstructionToMatch(std::string instructionStr)
        : matchAddr(0), instructionStr(instructionStr), normalizedInstructionStr(instructionStr), isAnyInst(false), hasWilcard(false), size(0)
    {
        NormalizeInstructionText(normalizedInstructionStr);
        const char* insChar = normalizedInstructionStr.c_str();
        size = strlen(insChar);
        if (size == 3 && normalizedInstructionStr[0] == '?' && normalizedInstructionStr[1] == '?' && normalizedInstructionStr[2] == '?')
        {
            isAnyInst = true;
        }
        else if (normalizedInstructionStr.find("?") != std::wstring::npos)
        {
            hasWilcard = true;
        }
    }
};

struct SequenceInstructions {
    // Armazena as sequências de instruções normalizadas
    std::vector<InstructionToMatch> instruction_sequences;
    int matchOcurrenceIndex;
    int matchCount;

    // Callback a ser chamado quando uma sequência for encontrada
    SeqMatchCallbackFunc match_callback;

    // Construtor para inicializar o contexto com IDs
    SequenceInstructions(const std::vector<std::string>& instruction_seq, SeqMatchCallbackFunc callback, int matchOcurrenceIndex = -1)
        : instruction_sequences(), matchOcurrenceIndex(matchOcurrenceIndex), matchCount(0), match_callback(callback)
    {
        for (const auto& instruction : instruction_seq)
        {
            instruction_sequences.push_back(InstructionToMatch(instruction));
        }
    }
};

class InstructionSequenceDetector
{
public:
    InstructionSequenceDetector();
    ~InstructionSequenceDetector();

    // Adiciona uma nova sequência de instruções para detecção
    void AddInstructionSequence(const std::vector<std::string>& sequence, SeqMatchCallbackFunc matchCallBack = nullptr, int matchOcurrenceIndex = -1);

    // Inicializa a estrutura necessária (deve ser chamado após adicionar todas as sequências)
    void Initialize();

    // Função de instrumentação que deve ser chamada para cada instrução
    VOID Instruction(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_text);
    VOID InstructionTrace(INS ins, VOID* v);
    VOID seqInstCB(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr);
    static VOID seqInstCBWrapper(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr, InstructionSequenceDetector* self);

    static VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v);
    static VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v);

    // Implementações não estáticas
    VOID ThreadStartImpl(THREADID tid, CONTEXT* ctxt, INT32 flags);
    VOID ThreadFiniImpl(THREADID tid, const CONTEXT* ctxt, INT32 code);

private:
    // Estrutura para armazenar dados específicos da thread
    struct ThreadData
    {
        std::deque<InstrumentedInstruction> instruction_queue;
    };

    // Key para o Thread-Local Storage
    TLS_KEY tls_key;

    // Armazena as sequências de instruções normalizadas
    std::vector<SequenceInstructions> instruction_sequences;

    // Tamanho máximo das sequências (para a fila)
    size_t max_sequence_length;

    // Métodos auxiliares
    ThreadData* GetThreadData();
};

#endif // INSTRUCTION_SEQUENCE_DETECTOR_H
