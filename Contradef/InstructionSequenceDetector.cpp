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

BOOL InstMatch(const InstrumentedInstruction& queue_inst, const InstructionToMatch& seq_inst) {
    if (seq_inst.isAnyInst) {
        return true; // Corresponde a qualquer instrução
    }

    if (seq_inst.hasWildcard) {
        if (queue_inst.size != seq_inst.size) {
            return false; // Tamanho diferente não pode corresponder
        }
        // Comparar caracter a caracter, considerando coringas
        for (size_t x = 0; x < seq_inst.size; ++x) {
            if (seq_inst.normalizedInstructionStr[x] != '?' &&
                queue_inst.normalizedInstructionStr[x] != seq_inst.normalizedInstructionStr[x]) {
                return false; // Encontrou um caractere que não corresponde
            }
        }
        return true; // Todos os caracteres corresponderam
    }

    // Comparação direta quando não há coringas
    return queue_inst.normalizedInstructionStr == seq_inst.normalizedInstructionStr;
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

    SequenceInstructions* sequenceInstructions = new SequenceInstructions(sequence, matchCallBack, matchOcurrenceIndex);

    instruction_sequences.push_back(sequenceInstructions);

    // Atualizar o tamanho máximo da sequência
    if (sequenceInstructions->instruction_sequences.size() > max_sequence_length)
    {
        max_sequence_length = sequenceInstructions->instruction_sequences.size();
    }
}

VOID InstructionSequenceDetector::Instruction(ADDRINT ip, CONTEXT* ctxt, InstrumentedInstruction* ins_instr)
{

    ThreadData* tdata = GetThreadData();

    // Adicionar a instrução atual à fila (com limite fixo de tamanho)
    ins_instr->instAddr = ip;
    if (tdata->instruction_queue.size() == max_sequence_length) {
        tdata->instruction_queue.pop_front();
    }
    tdata->instruction_queue.push_back(ins_instr);

    // Comparar a fila de instruções com as sequências armazenadas
    for (auto* sequence : instruction_sequences)
    {

        size_t seq_length = sequence->instruction_sequences.size();
        if (tdata->instruction_queue.size() < seq_length) {
            continue; // Não há instruções suficientes na fila para realizar a comparação
        }

        // Comparar a sequência da fila com a sequência armazenada
        auto start_it = tdata->instruction_queue.end() - seq_length;

        bool match = std::equal(start_it, tdata->instruction_queue.end(), sequence->instruction_sequences.begin(),
            [](const InstrumentedInstruction* queue_inst, const InstructionToMatch* seq_inst) {
                return InstMatch(*queue_inst, *seq_inst);
            });

        if (match) {
            // Sequência encontrada, chamar o callback
            sequence->matchCount++;
            if (sequence->match_callback && (sequence->matchOcurrenceIndex == -1 || sequence->matchOcurrenceIndex == sequence->matchCount)) {
                sequence->match_callback(PIN_ThreadId(), ip, ctxt, *sequence);
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
    std::string instStr = std::string(INS_Disassemble(ins));

    InstrumentedInstruction* ins_inst = new InstrumentedInstruction(instStr);

    InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);

    bool add = false;
    // Comparar a fila com as sequências
    for (auto* sequence : instruction_sequences)
    {
        for (auto* instruction : sequence->instruction_sequences)
        {
            if (InstMatch(*ins_inst, *instruction))
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


