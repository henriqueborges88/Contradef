#include "InstructionSequenceDetector.h"


void NormalizeInstructionText(std::string& instr)
{
    // Converter para minúsculas
    std::transform(instr.begin(), instr.end(), instr.begin(), ::tolower);

    // Remover espaços e tabulações
    instr.erase(std::remove_if(instr.begin(), instr.end(), ::isspace), instr.end());


    // Substituir endereços hexadecimais por "0xADDR"
    size_t pos = 0;
    while ((pos = instr.find("0x", pos)) != std::string::npos)
    {
        size_t end = pos + 2; // Começar após "0x"

        // Verificar se há pelo menos 6 caracteres hexadecimais após "0x"
        size_t hexCount = 0;
        while (end < instr.size() && std::isxdigit(instr[end]))
        {
            ++end;
            ++hexCount;
        }

        // Verifica se o endereço hexadecimal é suficientemente longo (por exemplo, 6 ou mais dígitos)
        if (hexCount >= 6)
        {
            instr.replace(pos, end - pos, "0xADDR");
            pos += 6; // Avançar após "0xADDR"
        }
        else
        {
            pos = end; // Avançar para a próxima posição
        }
    }
}

InstructionSequenceDetector::InstructionSequenceDetector()
    : max_sequence_length(0)
{
    tls_key = PIN_CreateThreadDataKey(NULL);
}

InstructionSequenceDetector::~InstructionSequenceDetector()
{
    // Nada a destruir explicitamente
}

void InstructionSequenceDetector::AddInstructionSequence(const std::vector<std::string>& sequence, SeqMatchCallbackFunc matchCallBack, int matchOcurrenceIndex)
{

    SequenceInstructions sequenceInstructions = SequenceInstructions(sequence, matchCallBack, matchOcurrenceIndex);

    instruction_sequences.push_back(sequenceInstructions);

    // Atualizar o tamanho máximo da sequência
    if (sequenceInstructions.instruction_sequences.size() > max_sequence_length)
    {
        max_sequence_length = sequenceInstructions.instruction_sequences.size();
    }
}


VOID InstructionSequenceDetector::Instruction(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr)
{
    ThreadData* tdata = GetThreadData();

    // Adicionar à fila
    ins_instr->instAddr = ip;
    tdata->instruction_queue.push_back(*ins_instr);

    // Remover instruções antigas se necessário
    if (tdata->instruction_queue.size() > max_sequence_length)
    {
        if (!tdata->instruction_queue.empty()) {
            tdata->instruction_queue.pop_front();
        }
    }

    // Comparar a fila com as sequências
    for (auto& sequence : instruction_sequences)
    {
        size_t seq_length = sequence.instruction_sequences.size();

        if (tdata->instruction_queue.size() >= seq_length)
        {
            auto start_it = tdata->instruction_queue.end() - seq_length;
            std::vector<InstrumentedInstruction> queue_sequence(start_it, tdata->instruction_queue.end());

            // Comparar as sequências
            bool match = true;
            for (size_t i = 0; i < seq_length; ++i)
            {
                if (!sequence.instruction_sequences[i].isAnyInst)
                {
                    if (sequence.instruction_sequences[i].hasWilcard)
                    {
                        if (queue_sequence[i].size != sequence.instruction_sequences[i].size)
                        {
                            match = false;
                            break;
                        }
                        for (size_t x = 0; x < sequence.instruction_sequences[i].size; ++x)
                        {
                            if (sequence.instruction_sequences[i].normalizedInstructionStr[x] != '?' && queue_sequence[i].normalizedInstructionStr[x] != sequence.instruction_sequences[i].normalizedInstructionStr[x])
                            {
                                match = false;
                                break;
                            }
                        }
                    }
                    else if (queue_sequence[i].normalizedInstructionStr != sequence.instruction_sequences[i].normalizedInstructionStr)
                    {
                        match = false;
                        break;
                    }
                }
                sequence.instruction_sequences[i].matchAddr = queue_sequence[i].instAddr;
            }

            if (match)
            {
                // Sequência encontrada, chamar o callback
                sequence.matchCount++;
            }
            if (match && sequence.match_callback && (sequence.matchOcurrenceIndex == -1 || sequence.matchOcurrenceIndex == sequence.matchCount))
            {
                // Sequência encontrada, chamar o callback
                sequence.match_callback(PIN_ThreadId(), ip, ctxt, sequence);
            }
        }
    }
}

VOID InstructionSequenceDetector::seqInstCB(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr)
{
    Instruction(ip, ctxt, ins_instr);
}

VOID InstructionSequenceDetector::seqInstCBWrapper(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr, InstructionSequenceDetector* self)
{
    // Chama o método não estático da classe
    self->seqInstCB(ip, ctxt, ins_instr);
}

VOID InstructionSequenceDetector::InstructionTrace(INS ins, VOID* v)
{
    std::string* instStr = new std::string(INS_Disassemble(ins));

    InstrumentedInstruction* ins_inst = new InstrumentedInstruction(*instStr);

    InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);

    bool add = false;
    // Comparar a fila com as sequências
    for (const auto& sequence : instruction_sequences)
    {
        for (const auto& instruction : sequence.instruction_sequences)
        {
            if (ins_inst->normalizedInstructionStr == instruction.normalizedInstructionStr)
            {
                add = true;
                break;
            }
        }
        if (add) // Interrompe o loop externo se o valor foi achado
        {
            break;
        }
    }

    if (add) 
    {
        INS_InsertCall(ins,
            IPOINT_BEFORE,
            AFUNPTR(seqInstCBWrapper),
            IARG_INST_PTR,
            IARG_CONTEXT,
            IARG_PTR, ins_inst,
            IARG_PTR, detector,        // Passando o ponteiro da instância da classe
            IARG_END);
    }
}

VOID InstructionSequenceDetector::ThreadStartImpl(THREADID tid, CONTEXT* ctxt, INT32 flags)
{
    ThreadData* tdata = new ThreadData();
    PIN_SetThreadData(tls_key, tdata, tid);
}

VOID InstructionSequenceDetector::ThreadFiniImpl(THREADID tid, const CONTEXT* ctxt, INT32 code)
{
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, tid));
    delete tdata;
}

VOID InstructionSequenceDetector::ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v)
{
    InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);

    if (detector)
    {
        detector->ThreadStartImpl(tid, ctxt, flags);
    }
}

VOID InstructionSequenceDetector::ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code, VOID* v)
{
    InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);

    if (detector)
    {
        detector->ThreadFiniImpl(tid, ctxt, code);
    }
}

void InstructionSequenceDetector::Initialize()
{
    // Registrar as funções de callback estáticas
    PIN_AddThreadStartFunction(ThreadStart, this);
    PIN_AddThreadFiniFunction(ThreadFini, this);
}

InstructionSequenceDetector::ThreadData* InstructionSequenceDetector::GetThreadData()
{
    THREADID tid = PIN_ThreadId();
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, tid));
    return tdata;
}


