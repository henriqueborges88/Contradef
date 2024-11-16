#pragma once
#ifndef INSTRUCTION_SEQUENCE_DETECTOR_H
#define INSTRUCTION_SEQUENCE_DETECTOR_H

#include "pin.H"
#include <iostream>
#include <vector>
#include <deque>
#include <algorithm>
#include <unordered_map>

class InstructionSequenceDetector
{
public:
    InstructionSequenceDetector();
    ~InstructionSequenceDetector();

    // Adiciona uma nova sequência de instruções para detecção
    void AddInstructionSequence(const std::vector<std::string>& sequence);

    // Inicializa a estrutura necessária (deve ser chamado após adicionar todas as sequências)
    void Initialize();

    // Função de instrumentação que deve ser chamada para cada instrução
    VOID Instruction(ADDRINT ip, CONTEXT* ctxt, std::string* ins_text);
    VOID InstructionSequenceDetector::InstructionTrace(INS ins, VOID* v);
    VOID InstructionSequenceDetector::seqInstCB(ADDRINT ip, CONTEXT* ctxt, std::string* disassemble_info);
    static VOID seqInstCBWrapper(ADDRINT ip, CONTEXT* ctxt, std::string* disassemble_info, InstructionSequenceDetector* self);

    // Funções de gerenciamento de threads
    VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags);
    VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code);

    // Define o callback a ser chamado quando uma sequência for encontrada
    void SetMatchCallback(VOID(*callback)(THREADID tid, ADDRINT ip, CONTEXT* ctxt, const std::vector<std::string>& sequence));

    void NormalizeInstructionText(std::string& instr);

private:
    // Estrutura para armazenar dados específicos da thread
    struct ThreadData
    {
        std::deque<std::string> instruction_queue;
    };

    // Key para o Thread-Local Storage
    TLS_KEY tls_key;

    // Armazena as sequências de instruções normalizadas
    std::vector<std::vector<std::string>> instruction_sequences;

    // Tamanho máximo das sequências (para a fila)
    size_t max_sequence_length;

    // Callback a ser chamado quando uma sequência for encontrada
    VOID(*match_callback)(THREADID tid, ADDRINT ip, CONTEXT* ctxt, const std::vector<std::string>& sequence);

    // Métodos auxiliares
    ThreadData* GetThreadData();
};

#endif // INSTRUCTION_SEQUENCE_DETECTOR_H
