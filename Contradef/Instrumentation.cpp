#include "Instrumentation.h"

BOOL IsMainExecutable(ADDRINT address) {
    PIN_LockClient();
    IMG img = IMG_FindByAddress(address);
    if (IMG_Valid(img)) {
        if (IMG_IsMainExecutable(img)) {
            PIN_UnlockClient();
            return TRUE;
        }
    }
    PIN_UnlockClient();
    return FALSE;
}

ADDRINT GetRtnAddr(ADDRINT instAddress) {
    PIN_LockClient();
    RTN rtnCall = RTN_FindByAddress(instAddress);
    ADDRINT rtnAddress = 0;
    if (RTN_Valid(rtnCall)) {
        rtnAddress = RTN_Address(rtnCall);
    }
    PIN_UnlockClient();
    return rtnAddress;
}

void LogRoutineInstructions(RTN rtnCaller, std::stringstream& stringStream, ADDRINT rtnTargetAddress) {

    stringStream << "    Nome da rotina chamadora: " << RTN_Name(rtnCaller) << std::endl;

    if (!KnobDetailLevel.Value()) {
        return; // Não faz nada se o detalhamento não estiver ativado
    }

    stringStream << "    Instruções da rotina chamadora (" << RTN_Name(rtnCaller) << "):" << std::endl;

    RTN_Open(rtnCaller);

    std::deque<std::string> instructionsBuffer;
    bool found = false;
    size_t const bufferCapacity = 40; // Total para armazenar 20 antes e 20 depois

    // Percorre todas as instruções da rotina
    INS ins = RTN_InsHead(rtnCaller);
    for (; INS_Valid(ins); ins = INS_Next(ins)) {
        std::stringstream formattedInstructionStream;
        ADDRINT insAddress = INS_Address(ins);
        std::string disassembledInstruction = INS_Disassemble(ins);
        BOOL isCallToTarget = FALSE;
        // Adiciona informações adicionais se for uma chamada direta
        if (INS_IsCall(ins) && INS_IsDirectCall(ins)) {
            ADDRINT targetAddr = INS_DirectBranchOrCallTargetAddress(ins);
            RTN targetRtn = RTN_FindByAddress(targetAddr);
            if (RTN_Valid(targetRtn)) {
                std::string targetName = RTN_Name(targetRtn);
                disassembledInstruction += "                           ; " + targetName;
            }
            if (targetAddr == rtnTargetAddress) {
                isCallToTarget = TRUE;
            }
        }

        // Formata a instrução com seu endereço usando stringstream
        std::string fcnPointer = "    ";
        if (isCallToTarget) {
            fcnPointer = " -> ";
        }
        formattedInstructionStream << "        " << std::hex << insAddress << std::dec << "    |" << fcnPointer << disassembledInstruction;
        std::string formattedInstruction = formattedInstructionStream.str();

        // Adiciona a instrução atual ao buffer
        instructionsBuffer.push_back(formattedInstruction);

        // Mantém o buffer com no máximo 40 instruções
        if (instructionsBuffer.size() > bufferCapacity) {
            instructionsBuffer.pop_front();
        }

        // Verifica se a instrução corrente é a procurada
        if (isCallToTarget) {
            found = true;
            break; // Encontrou a instrução, interrompe o loop
        }
    }

    if (found) {
        // A instrução específica foi encontrada, processa o buffer
        std::deque<std::string> outputBuffer;

        // Adiciona as instruções anteriores
        size_t count = 0;
        auto it = instructionsBuffer.begin();
        while (it != instructionsBuffer.end() && count < 20) {
            outputBuffer.push_back(*it);
            ++it;
            ++count;
        }

        // Adiciona a instrução encontrada e as próximas 20 instruções
        outputBuffer.push_back(*it); // Instrução específica
        ins = INS_Next(ins); // Move para a próxima instrução
        count = 0;
        for (; INS_Valid(ins) && count < 20; ins = INS_Next(ins), ++count) {
            std::stringstream lineStream;
            std::string disassembledInstruction = INS_Disassemble(ins);
            ADDRINT insAddress = INS_Address(ins);
            lineStream << "        " << std::hex << insAddress << std::dec << "    |    " << disassembledInstruction;
            outputBuffer.push_back(lineStream.str());
        }

        // Output das instruções selecionadas
        for (const auto& line : outputBuffer) {
            stringStream << line << std::endl;
        }
    }
    else {
        stringStream << "        Erro: Endereço " << std::hex << rtnTargetAddress << std::dec << " não encontrado na rotina " << RTN_Name(rtnCaller) << ". A invocação pode ter sido de uma chamada indireta." << std::endl;
    }

    RTN_Close(rtnCaller);
}