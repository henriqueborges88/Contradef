#include "InstructionSequences.h"
#include "ContextOperations.h"

void AddSequences(InstructionSequenceDetector& seq_detector, InstructionSequenceDetector& disassembly_seq_detector) {
	// Detector de sequência de instruções aplicado à instruções executadas (na ordem da execução)
	// Exemplo de uso
	//std::vector<std::vector<std::string>> sequence;
	//sequence.push_back(CreateInstVector("jz 0x000000000000"));
	//sequence.push_back(CreateInstVector("test word ptr [r?i+0xa], 0x2")); // "?" substitui qualquer valor
	//sequence.push_back(CreateInstVector("jmp 0x000000000000", "jnb 0x000000000000", "jb 0x000000000000")); // Qualquer dessas instruções é considerada
	//sequence.push_back(CreateInstVector("ja 0x000000000000"));
	//sequence.push_back(CreateInstVector("jnz 0x000000000000"));

	//seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback, nullptr, 10, 15); // O callback será disparado entre as ocorrências 10 e 15 da mesma sequência de instruções
	//seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback, nullptr, 34, 34); // O callback será disparado na ocorrência 34 da mesma sequência de instruções
	//seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback); // O callback SequenceMatchCallback será disparado em todas as ocorrências 


	// Detector de sequência de instruções aplicado ao código desmontado (na ordem da desmontagem)
	// Tecnica antidesmontagem
	std::vector<std::vector<std::string>> ad_same_target;
	ad_same_target.push_back(CreateInstVector("jz 0x000000000000"));
	ad_same_target.push_back(CreateInstVector("jnz 0x000000000000"));
	disassembly_seq_detector.AddInstructionSequence(ad_same_target, SequenceMatchAntiDebugSameTarget, &disassembly_seq_detector);

	// Tecnica antidesmontagem
	std::vector<std::vector<std::string>> ad_const_condition;
	ad_const_condition.push_back(CreateInstVector("xor eax, eax"));
	ad_const_condition.push_back(CreateInstVector("jz 0x000000000000"));
	disassembly_seq_detector.AddInstructionSequence(ad_const_condition, SequenceMatchAntiDebugConstCondition, &disassembly_seq_detector);

	if (IsStringInStrategyScope("Obsidium")) {
	
		// Antidbi - Simulando HW-BP (single step)
		std::vector<std::vector<std::string>> sequenceObsidiumBypass2;
		sequenceObsidiumBypass2.push_back(CreateInstVector("jmp 0x1b8d908404f"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("test eax, eax"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("jmp 0x1b8d9084058"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("jz 0x1b8d9084359"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("jmp 0x1b8d9084061"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("jmp 0x1b8d9084065"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("jmp 0x1b8d9084068"));
		sequenceObsidiumBypass2.push_back(CreateInstVector("nop"));
		seq_detector.AddInstructionSequence(sequenceObsidiumBypass2, SequenceObsidiumBypassMatchCallback2, nullptr);

		// Alternativa para a anterior (Antidbi)
		//std::vector<std::vector<std::string>> sequenceObsidiumBypass1;
		//sequenceObsidiumBypass1.push_back(CreateInstVector("jmp 0x1b8d90842fc"));
		//sequenceObsidiumBypass1.push_back(CreateInstVector("xor rdx, rdx"));
		//sequenceObsidiumBypass1.push_back(CreateInstVector("jp 0x1b8d9084304"));
		//sequenceObsidiumBypass1.push_back(CreateInstVector("mov eax, 0xf0e"));
		//sequenceObsidiumBypass1.push_back(CreateInstVector("jmp 0x1b8d908430d"));
		//seq_detector.AddInstructionSequence(sequenceObsidiumBypass1, SequenceObsidiumBypassMatchCallback, nullptr);

	}

}
