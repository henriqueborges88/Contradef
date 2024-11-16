#include "InstructionSequenceDetector.h"

InstructionSequenceDetector::InstructionSequenceDetector()
    : max_sequence_length(0), match_callback(nullptr)
{
    tls_key = PIN_CreateThreadDataKey(NULL);
}

InstructionSequenceDetector::~InstructionSequenceDetector()
{
    // Nada a destruir explicitamente
}

void InstructionSequenceDetector::AddInstructionSequence(const std::vector<std::string>& sequence)
{
    // Normalizar cada instrução na sequência
    std::vector<std::string> normalized_sequence;
    for (auto instr : sequence)
    {
        NormalizeInstructionText(instr);
        normalized_sequence.push_back(instr);
    }

    instruction_sequences.push_back(normalized_sequence);

    // Atualizar o tamanho máximo da sequência
    if (normalized_sequence.size() > max_sequence_length)
    {
        max_sequence_length = normalized_sequence.size();
    }
}

void InstructionSequenceDetector::Initialize()
{
    // Nenhuma inicialização adicional necessária no momento
}

VOID InstructionSequenceDetector::Instruction(ADDRINT ip, CONTEXT* ctxt, std::string* ins_text)
{
    ThreadData* tdata = GetThreadData();





    //if (ip == 140702179776952) {
    //    size_t seq_length = tdata->instruction_queue.size();

    //    auto start_it = tdata->instruction_queue.end() - seq_length;
    //    std::vector<std::string> queue_sequence(start_it, tdata->instruction_queue.end());

    //    // Comparar as sequências
    //    bool match = true;
    //    for (size_t i = 0; i < seq_length; ++i)
    //    {

    //        std::cout << std::hex << ip << std::dec << " - " << queue_sequence[i] << std::endl << std::flush;

    //    }
    //}





    // Adicionar à fila
    tdata->instruction_queue.push_back(*ins_text);

    // Remover instruções antigas se necessário
    if (tdata->instruction_queue.size() > max_sequence_length)
    {
        tdata->instruction_queue.pop_front();
    }

    // Comparar a fila com as sequências
    for (const auto& sequence : instruction_sequences)
    {
        size_t seq_length = sequence.size();

        if (tdata->instruction_queue.size() >= seq_length)
        {
            auto start_it = tdata->instruction_queue.end() - seq_length;
            std::vector<std::string> queue_sequence(start_it, tdata->instruction_queue.end());

            // Comparar as sequências
            bool match = true;
            for (size_t i = 0; i < seq_length; ++i)
            {
                if (queue_sequence[i] != sequence[i])
                {
                    match = false;
                    break;
                }
            }

            if (match && match_callback)
            {
                // Sequência encontrada, chamar o callback
                match_callback(PIN_ThreadId(), ip, ctxt, queue_sequence);
            }
        }
    }
}

VOID InstructionSequenceDetector::seqInstCB(ADDRINT ip, CONTEXT* ctxt, std::string* disassemble_info)
{
    Instruction(ip, ctxt, disassemble_info);
}

VOID InstructionSequenceDetector::seqInstCBWrapper(ADDRINT ip, CONTEXT* ctxt, std::string* disassemble_info, InstructionSequenceDetector* self)
{
    // Chama o método não estático da classe
    self->seqInstCB(ip, ctxt, disassemble_info);
}

VOID InstructionSequenceDetector::InstructionTrace(INS ins, VOID* v)
{
    std::string* instStr = new std::string(INS_Disassemble(ins));
    NormalizeInstructionText(*instStr);

    InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);

    INS_InsertCall(ins,
        IPOINT_BEFORE,
        AFUNPTR(seqInstCBWrapper),
        IARG_INST_PTR,
        IARG_CONTEXT,
        IARG_PTR, instStr,
        IARG_PTR, detector,        // Passando o ponteiro da instância da classe
        IARG_END);




    //// Comparar a fila com as sequências
    //for (const auto& sequence : instruction_sequences)
    //{
    //    for (const auto& instruction : sequence)
    //    {
    //        if (*instStr == instruction)
    //        {
    //            InstructionSequenceDetector* detector = reinterpret_cast<InstructionSequenceDetector*>(v);
    //            std::cout << *instStr << std::endl;
    //            INS_InsertCall(ins,
    //                IPOINT_BEFORE,
    //                AFUNPTR(seqInstCBWrapper),
    //                IARG_INST_PTR,
    //                IARG_CONTEXT,
    //                IARG_PTR, instStr,
    //                IARG_PTR, detector,        // Passando o ponteiro da instância da classe
    //                IARG_END);
    //        }
    //    }
    //}

}

VOID InstructionSequenceDetector::ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags)
{
    ThreadData* tdata = new ThreadData();
    PIN_SetThreadData(tls_key, tdata, tid);
}

VOID InstructionSequenceDetector::ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 code)
{
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, tid));
    delete tdata;
}

void InstructionSequenceDetector::SetMatchCallback(VOID(*callback)(THREADID tid, ADDRINT ip, CONTEXT* ctxt, const std::vector<std::string>& sequence))
{
    match_callback = callback;
}

void InstructionSequenceDetector::NormalizeInstructionText(std::string& instr)
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

InstructionSequenceDetector::ThreadData* InstructionSequenceDetector::GetThreadData()
{
    THREADID tid = PIN_ThreadId();
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, tid));
    return tdata;
}
