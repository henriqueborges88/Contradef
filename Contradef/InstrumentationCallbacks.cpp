#include "InstrumentationCallbacks.h"

VOID SequenceMatchCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    std::cout << "\n[CONTRADEF][CALLBACK]\n";

    SetZfToZero(ctxt);
}

VOID SequenceMatchAntiDebugSameTarget(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    InstructionSequenceDetector* disassembly_seq_detector = reinterpret_cast<InstructionSequenceDetector*>(v);
    std::deque<InstrumentedInstruction*> instruction_queue = disassembly_seq_detector->GetDisassembledInstructionQueue();

    InstrumentedInstruction* inst = instruction_queue[instruction_queue.size() - 2];
    InstrumentedInstruction* inst2 = instruction_queue[instruction_queue.size() - 1];

    std::string inststr = inst->instructionStr;
    std::string inststr2 = inst2->instructionStr;

    std::string subinst = inststr.substr(2, 15);
    std::string subinst2 = inststr2.substr(3, 15);

    if (subinst == subinst2 && (inst->instAddr + 2) == inst2->instAddr) {  // 2 = tamanho da instrução jz (SHORT JUMP)
        std::cout << "\n[CONTRADEF][ANTIDEBUG] Instruções de Salto com o Mesmo Alvo:" << std::endl;
        std::cout << "    " << std::hex << inst->instAddr << " | " << inststr << std::dec << std::endl;
        std::cout << "    " << std::hex << inst2->instAddr << " | " << inststr2 << std::dec << std::endl;
    }
}


VOID SequenceMatchAntiDebugConstCondition(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    InstructionSequenceDetector* disassembly_seq_detector = reinterpret_cast<InstructionSequenceDetector*>(v);
    std::deque<InstrumentedInstruction*> instruction_queue = disassembly_seq_detector->GetDisassembledInstructionQueue();

    InstrumentedInstruction* inst = instruction_queue[instruction_queue.size() - 2];
    InstrumentedInstruction* inst2 = instruction_queue[instruction_queue.size() - 1];

    std::string inststr = inst->instructionStr;
    std::string inststr2 = inst2->instructionStr;

    if ((inst->instAddr + 2) == inst2->instAddr) { // 2 = tamanho da instrução xor
        std::cout << "\n[CONTRADEF][ANTIDEBUG] Instruções com Condição Constante:" << std::endl;
        std::cout << "    " << std::hex << inst->instAddr << " | " << inststr << std::dec << std::endl;
        std::cout << "    " << std::hex << inst2->instAddr << " | " << inststr2 << std::dec << std::endl;
    }
}


VOID SequenceMatchPauseCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    OS_PROCESS_ID pid = PIN_GetPid();
    std::cout << "Sequencia de instrucoes identificada, endereco de instrucao " << std::hex << ip << std::dec << ", corresponde a funcao: " << GetFunctionFromAddress(ip, pid).functionName << std::endl;
    PauseAtAddress(ip);
}

VOID SequenceObsidiumBypassMatchCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    std::cout << "\n[CONTRADEF][CALLBACK] Obsidium bypass\n";
    std::cout << "[CONTRADEF] APLICANDO CONTRAMEDIDA NO INDICE " << sequence.matchCount << std::endl;

    PIN_SetContextReg(ctxt, REG_RAX, 0);

    //SetZfToOne(ctxt);


    //// Obter os flags atuais
    //ADDRINT flags = PIN_GetContextReg(ctxt, REG_GFLAGS);

    //// Verificar o valor atual do ZF
    //BOOL zf_before = (flags & (1 << 6)) != 0;
    ////std::cout << "1 - ZF ->" << zf_before << std::endl;

    //if (zf_before == 0) {

    //    // Para setar o ZF para 1:
    //    flags |= (1 << 6); // Seta o bit 6 (ZF) para 1
    //    std::cout << "[CONTRADEF] APLICANDO CONTRAMEDIDA NO INDICE " << sequence.matchCount << std::endl;

    //    // Para setar o ZF para 0:
    //    //flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

    //    // Atualizar os flags
    //    PIN_SetContextReg(ctxt, REG_GFLAGS, flags);

    //}

}



VOID SequenceObsidiumBypassMatchCallback2(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    std::cout << "\n[CONTRADEF] Obsidium bypass\n";

    ADDRINT pc = PIN_GetContextReg(ctxt, REG_INST_PTR);

    PIN_SetContextReg(ctxt, REG_DR0, pc);   // 0x1b8d9084068
    PIN_SetContextReg(ctxt, REG_DR7, 0x501);  // local-exec enable DR0
    PIN_SetContextReg(ctxt, REG_DR6, 0x1);    // B0 = 1 (indicando que DR0 disparou)

    PIN_SetContextReg(ctxt, REG_INST_PTR, pc); 

    EXCEPTION_INFO exc;
    PIN_InitExceptionInfo(&exc, EXCEPTCODE_DBG_SINGLE_STEP_TRAP, pc);

    PIN_RaiseException(ctxt, tid, &exc);
}

// Exemplo de alteração de floxo de execução
VOID SequenceThemidaBypassMatchCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence, VOID* v)
{
    std::cout << "\n[CONTRADEF] Themida bypass\n";
    //ADDRINT nip = NextIpFrom(ip);
    //std::cout << "[CONTRADEF][CALLBACK] IP: " << ip << " Next IP: " << nip << std::endl;
    //JumpToAddress(ctxt, nip);

    ADDRINT flags = PIN_GetContextReg(ctxt, REG_GFLAGS);

    const ADDRINT CF = 0x1, ZF = 0x40;
    flags &= ~(CF | ZF);
}