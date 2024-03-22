#include "InstNtQueryInformationProcess.h"

/*
A função NtQueryInformationProcess é uma chamada de sistema do Windows, e a instrumentação para ela pode ser mais complexa do que para funções de API padrão.

Info sobre a função:
https://undocumented.ntinternals.net/index.html?page=UserMode%2FUndocumented%20Functions%2FNT%20Objects%2FProcess%2FNtQueryInformationProcess.html
https://pubhtml5.com/dtiq/ufpk/Windows_Kernel_Programming/274
https://forums.codeguru.com/showthread.php?371338-debugger-check-via-PEB
https://copyprogramming.com/howto/ntqueryinformationprocess-function-winternl-h
https://anti-debug.checkpoint.com/techniques/debug-flags.html
https://www.youtube.com/watch?v=WlE8abc8V-4
https://community.osr.com/discussion/228192

Verificar se pode ser utilizado para obter os argumentos de execução do processo e ver se está sendo debugado:
https://www.bordergate.co.uk/argument-spoofing/

*/


// Definição do PROCESSINFOCLASS e ProcessBasicInformation
// Mais informação sobre classes de comportamento evasivo -> https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess
/*
Value	Meaning
ProcessBasicInformation
0
Retrieves a pointer to a PEB structure that can be used to determine whether the specified process is being debugged, and a unique value used by the system to identify the specified process.
Use the CheckRemoteDebuggerPresent and GetProcessId functions to obtain this information.

ProcessDebugPort
7
Retrieves a DWORD_PTR value that is the port number of the debugger for the process. A nonzero value indicates that the process is being run under the control of a ring 3 debugger.
Use the CheckRemoteDebuggerPresent or IsDebuggerPresent function.

ProcessWow64Information
26
Determines whether the process is running in the WOW64 environment (WOW64 is the x86 emulator that allows Win32-based applications to run on 64-bit Windows).
Use the IsWow64Process2 function to obtain this information.

ProcessImageFileName
27
Retrieves a UNICODE_STRING value containing the name of the image file for the process.
Use the QueryFullProcessImageName or GetProcessImageFileName function to obtain this information.

ProcessBreakOnTermination
29
Retrieves a ULONG value indicating whether the process is considered critical.
Note  This value can be used starting in Windows XP with SP3. Starting in Windows 8.1, IsProcessCritical should be used instead.

ProcessTelemetryIdInformation
64
Retrieves a PROCESS_TELEMETRY_ID_INFORMATION_TYPE value that contains metadata about a process.

ProcessSubsystemInformation
75
Retrieves a SUBSYSTEM_INFORMATION_TYPE value indicating the subsystem type of the process. The buffer pointed to by the ProcessInformation parameter should be large enough to hold a single SUBSYSTEM_INFORMATION_TYPE enumeration.


Extraído de:
https://github.com/winsiderss/systeminformer/blob/ae0cdac96f9c752c881a801af03f5b74deb51fa9/phnt/include/ntpsapi.h#L110
https://ntdoc.m417z.com/processinfoclass

Outras estruturas: https://www.cin.ufpe.br/~frsn/arquivos/GnuWin32/include/ddk/ntapi.h
*/

LONG InstNtQueryInformationProcess::retVal;
ADDRINT InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS;
DWORD InstNtQueryInformationProcess::NtGlobalFlag = -1;
std::map<CallContextKey, CallContext*> InstNtQueryInformationProcess::callContextMap;
UINT32 InstNtQueryInformationProcess::imgCallId = 0;
UINT32 InstNtQueryInformationProcess::fcnCallId = 0;
Notifier* InstNtQueryInformationProcess::globalNotifierPtr;

VOID InstNtQueryInformationProcess::CheckPrintConditions(CallContext* callContext) {
    CheckPrintConditionsCC(callContext, &InstNtQueryInformationProcess::callContextMap, InstNtQueryInformationProcess::globalNotifierPtr);
}

// Callbacks que serão chamados quando um evento for notificado
VOID InstNtQueryInformationProcess::HandleInstructionEvent(const EventData* data, void* context) {
    HandleInstructionEventCC(data, context, &InstNtQueryInformationProcess::callContextMap, InstNtQueryInformationProcess::globalNotifierPtr);
}

VOID InstNtQueryInformationProcess::HandleTraceEvent(const EventData* data, void* context) {
    HandleTraceEventCC(data, context, &InstNtQueryInformationProcess::callContextMap, InstNtQueryInformationProcess::globalNotifierPtr);
}

VOID InstNtQueryInformationProcess::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT returnAddress, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    NtQueryInformationProcessArgs args;
    args.ProcessHandle = ProcessHandle;
    args.ProcessInformationClass = ProcessInformationClass;
    args.ProcessInformation = ProcessInformation;
    args.ProcessInformationLength = ProcessInformationLength;
    args.ReturnLength = ReturnLength;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, &args);

    // Criando observadores dinamicamente com o contexto correto
    auto* instructionObserver = new Observer(InstNtQueryInformationProcess::HandleInstructionEvent, callContext);
    auto* traceObserver = new Observer(InstNtQueryInformationProcess::HandleTraceEvent, callContext);

    // Salvando os observadores no CallContext para futura limpeza
    callContext->instructionObserver = instructionObserver;
    callContext->traceObserver = traceObserver;

    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;

    // Anexa os observers ao notificador
    InstNtQueryInformationProcess::globalNotifierPtr->Attach(instructionObserver);
    InstNtQueryInformationProcess::globalNotifierPtr->Attach(traceObserver);

}

VOID InstNtQueryInformationProcess::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, RTN* rtn, CONTEXT* ctx, ADDRINT* retValAddr, ADDRINT returnAddress, ADDRINT ProcessHandle, ADDRINT ProcessInformationClass, ADDRINT ProcessInformation, ADDRINT ProcessInformationLength, ADDRINT ReturnLength) {

    if (!IsMainExecutable(returnAddress)) {
        return;
    }

    // Instrumentar funçao
    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);
    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);

        CallContext* callContext = it->second;
        // Registrar parámetros
        const NtQueryInformationProcessArgs* args = reinterpret_cast<NtQueryInformationProcessArgs*>(callContext->functionArgs);

        // ADDRINT ProcessInformation é alterado no retorno, posivelmente por alguma manipulação do registrador durante a execução da funçaõ.

        if (ProcessInformationClass == ProcessBasicInformation) {
            // Verifica se ProcessInformation é um ponteiro válido
            if (args->ProcessInformation != NULL) {
                PROCESS_BASIC_INFORMATION* pbi = reinterpret_cast<PROCESS_BASIC_INFORMATION*>(args->ProcessInformation);

                // 32 bits
                // DWORD ptr = (DWORD)pbi->PebBaseAddress;
                // ptr |= 0x68;
                // DWORD* NtGlobalFlagPtr = reinterpret_cast<DWORD*>(ptr);

                // 64 bits
                DWORD_PTR pebBaseAddress = reinterpret_cast<DWORD_PTR>(pbi->PebBaseAddress);
                if (PIN_CheckReadAccess(reinterpret_cast<void*>(pebBaseAddress))) {
                    // Considerando que 0xBC é o offset para NtGlobalFlag no PEB
                    DWORD_PTR NtGlobalFlagPtrAddress = pebBaseAddress + 0xBC;
                    DWORD* NtGlobalFlagPtr = reinterpret_cast<DWORD*>(NtGlobalFlagPtrAddress);

                    if (PIN_CheckReadAccess(reinterpret_cast<void*>(NtGlobalFlagPtr))) {
                        TARGET_MEMORY_ADDRESS = reinterpret_cast<ADDRINT>(NtGlobalFlagPtr);
                        NtGlobalFlag = *NtGlobalFlagPtr; // Salvar o valor de NtGlobalFlag

                        // Criar a função de instrumentação apenas uma vez e enviar o ponteiro para o endereço de memoria dque deve ser verificado. En
                        INS_AddInstrumentFunction(Instruction, callContext);
                    }
                }

            }
        }
    }
    
    retVal = *retValAddr;

    fcnCallId++;

}


/// transferir funçoes para o main
// Variável de estado para rastrear o progresso na identificação da sequência




// Lista de registradores permitidos para instrumentação
REG init[] = {
    REG_GAX,
    REG_GBX,
    REG_GCX,
    REG_GDX,
    REG_GSI,
    REG_GDI,
    REG_GBP,
    REG_SEG_SS,
    REG_SEG_CS,
    REG_SEG_DS,
    REG_SEG_ES,
    REG_SEG_FS,
    REG_SEG_GS,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
};
std::set<REG> permittedRegisters(init, init + sizeof(init) / sizeof(init[0]));

// Função para verificar se um registrador está na lista permitida
bool InstNtQueryInformationProcess::IsRegisterAllowed(const REG& regName) {
    return permittedRegisters.find(regName) != permittedRegisters.end();
}

VOID FetchInsCode(INS ins) {
    
    PIN_LockClient();

    // Obter o endereço da instrução
    ADDRINT address = INS_Address(ins);

    // Definir um buffer para armazenar o código binário da instrução
    std::vector<UINT8> buffer(15); //15 bytes -> x64bits Tamanho máximo para uma instrução x86/64

    EXCEPTION_INFO exceptInfo;

    // Buscar o código binário da instrução
    if (PIN_FetchCode(reinterpret_cast<UINT8*>(&buffer[0]), (VOID*)address, 15, &exceptInfo)) {
        std::cout << "Instrução em 0x" << std::hex << address << ": ";
        for (UINT8 byte : buffer) {
            std::cout << std::hex << static_cast<unsigned>(byte);
        }
        std::cout << std::endl;
    }
    //else {
    //    std::cout << "Falha ao buscar o código da instrução em 0x" << std::hex << address << std::endl;
    //}

    PIN_UnlockClient();

}

// Callback de análise para ler um valor de memória
VOID InstNtQueryInformationProcess::ReadMemoryValue(THREADID tid, CallContext* callContext, CONTEXT* ctxt, INS ins, ADDRINT instAddress, ADDRINT memoryAddress, UINT32 readSize) {
    
    if (callContext == nullptr) {
        return;
    }

    const NtQueryInformationProcessArgs* args = reinterpret_cast<NtQueryInformationProcessArgs*>(callContext->functionArgs);
    std::stringstream& stringStream = callContext->stringStream;

    if (InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS != 0 && InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS == memoryAddress) {
        if (INS_Opcode(ins) != XED_ICLASS_CMP && INS_Opcode(ins) != XED_ICLASS_CMOVZ && INS_Opcode(ins) != XED_ICLASS_CMOVNZ) {
            //return;
        }
        
        int numOperands = INS_OperandCount(ins);
        for (int i = 0; i < numOperands; ++i) {
            // Verificar se o operando é imediato
            // O valor de memória procurado é dinamico, então não pode estar como valor imediato (constante)
            // Porem, o operando a ser comparado com o valor em memoria pode conter o valor 0
            if (INS_OperandIsImmediate(ins, i)) {
                ADDRINT imm = INS_OperandImmediate(ins, i);
                //std::cout << "Valor do operando imediato: " << imm << std::endl;
            }

            // Verificar se o operando é do tipo registrador
            //if (INS_OperandIsReg(ins, i)) {
            //    REG reg = INS_OperandReg(ins, i);
            //    ADDRINT regValue = PIN_GetContextReg(ctxt, reg);

            //    // Imprimir o valor do registrador em hexadecimal
            //    //std::cout << std::hex << std::showbase;  // Configurar o fluxo de saída para hexadecimal
            //    std::cout << "Valor do registrador " << REG_StringShort(reg) << ": " << regValue << std::endl;
            //    //std::cout << std::dec;  // Voltar para a formatação decimal para futuras impressões
            //}

            // Verificar se o operando é do tipo memória
            if (INS_OperandIsMemory(ins, i)) {
                // Se necessário inserir alguma rotina de verificação
            }

        }


        // Realizar a leitura do valor em memoria
        int valueAtNtGlobalFlagAddress = *reinterpret_cast<int*>(TARGET_MEMORY_ADDRESS);

        if (valueAtNtGlobalFlagAddress != NtGlobalFlag) {
            return;
        }

        PIN_LockClient();
        
        IMG img = IMG_FindByAddress(instAddress);
        std::string moduleName = ExtractModuleName(IMG_Name(img));
        if (toUpperCase(moduleName) == "NTDLL.DLL") {
            // Se a leitura ocorre em ntdll.dll desconsidera. Alguns protetores podem carregar funções em modulos diferentes ao principal, se for o caso, desativar o return
            return;
        }

        // Obter o ponteiro para a pilha atual
        ADDRINT* stackPtr;
        PIN_GetContextRegval(ctxt, REG_STACK_PTR, reinterpret_cast<UINT8*>(&stackPtr));

        // Obter o endereço de retorno da pilha
        ADDRINT returnAddr = *stackPtr;

        // Obter a disassemblagem da instrução
        std::string disassembledInstruction = INS_Disassemble(ins);
        
        // Obter a RTN da instrução atual
        RTN rtn = INS_Rtn(ins);

        ADDRINT rtnAddress = RTN_Address(rtn);
        stringStream << std::endl << "[+] NtQueryInformationProcess [NtGlobalFlag]..." << std::endl;
        stringStream << "    Nome do módulo: " << IMG_Name(img) << std::endl;
        stringStream << "    Thread: " << tid << std::endl;
        stringStream << "    Endereço da rotina: " << std::hex << rtnAddress << std::dec << std::endl;
        if (RTN_Valid(rtn)) {
            stringStream << "    Nome da rotina: " + RTN_Name(rtn) << std::endl;
        }
        stringStream << "    Valor de retorno (NtQueryInformationProcess): " << retVal << std::endl;
        stringStream << "    Instrução: " << disassembledInstruction << std::endl;
        stringStream << "    Endereço da instrução: " << std::hex << std::showbase << instAddress << std::dec << std::endl;
        stringStream << "    Endereço do valor em memória: " << std::hex << std::showbase << memoryAddress << std::dec << std::endl;
        stringStream << "    Valor da flag NtGlobalFlag: " << valueAtNtGlobalFlagAddress << std::endl;

        // Instrumentar todas as instruções da rotina chamadora
        std::string callerFcn = "";
        RTN rtnCaller = RTN_FindByAddress(returnAddr);
        if (RTN_Valid(rtnCaller)) {
            RTN_Open(rtnCaller);
            
            stringStream << "    Nome da rotina chamadora (possível): " + RTN_Name(rtnCaller) << std::endl;
            stringStream << "    Instruções da rotina chamadora (" << RTN_Name(rtnCaller) << "):" << std::endl;
            for (INS ins = RTN_InsHead(rtnCaller); INS_Valid(ins); ins = INS_Next(ins)) {
                std::string disassembledInstruction = INS_Disassemble(ins);
                ADDRINT insAddress = INS_Address(ins);
                if (INS_IsCall(ins)) {
                    // Assume que a chamada é direta para simplificar, para chamadas indiretas seria necessário mais contexto
                    if (INS_IsDirectCall(ins)) {
                        ADDRINT targetAddr = INS_DirectBranchOrCallTargetAddress(ins);
                        RTN targetRtn = RTN_FindByAddress(targetAddr);
                        if (RTN_Valid(targetRtn)) {
                            std::string targetName = RTN_Name(targetRtn);
                            disassembledInstruction = disassembledInstruction + "                           ; " + targetName; // Supondo que InstuctionInfo tenha esse campo
                        }
                    }
                }
                stringStream << "        " << std::hex << insAddress << std::dec << " | " << disassembledInstruction << std::endl;
            }

            RTN_Close(rtnCaller);
        }
        PIN_UnlockClient();
        callContext->defaultPrintReady = true;
        InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS = 0;
    }
}

// (DESATIVADO) Callback de análise para ler um valor de um registrador 
VOID InstNtQueryInformationProcess::RegContentAnalysisRoutine(CONTEXT* ctxt, REG regToInspect) {
    ADDRINT regValue = PIN_GetContextReg(ctxt, regToInspect);
    //std::cout << "Valor do registrador: " << REG_StringShort(regToInspect) << " = " << regValue << std::endl;
    if (InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS != 0 && InstNtQueryInformationProcess::TARGET_MEMORY_ADDRESS == regValue) {
        std::cout << "-----Endereço de memória para o NtGlobalFlagPtr foi acessado em uma instrução CMP por meio do registrador " << REG_StringShort(regToInspect) << "com valor " << regValue << std::endl;
    }
}

VOID InstNtQueryInformationProcess::Instruction(INS ins, VOID* v) {
    /*
    Procurar em qual registrador é armazenado o valor NtGlobalFlagPtr e logo se tem um CMP com 0 na sequencia (cmp dword ptr [rbx], 0)
    */
    if (INS_IsMemoryRead(ins) ) { // 
        // CMP identificado

        // Obter a disassemblagem da instrução
        //std::string disassembledInstruction = INS_Disassemble(ins);
        // Imprimir a disassemblagem
        //std::cout << disassembledInstruction << std::endl;

        // Verificar se tem ao menos dois operandos
        if (INS_OperandCount(ins) > 1) {
            // Obter o número de operandos
            int numOperands = INS_OperandCount(ins);

            for (int i = 0; i < numOperands; ++i) {
                // Verificar se o operando é imediato
                // O valor de memória procurado é dinamico, então não pode estar como valor imediato (constante)
                // Porem, o operando a ser comparado com o valor em memoria pode conter o valor 0
                if (INS_OperandIsImmediate(ins, i)) {
                    ADDRINT imm = INS_OperandImmediate(ins, i);
                }

                // Verificar se o operando é do tipo registrador
                if (INS_OperandIsReg(ins, i)) {
                    REG reg = INS_OperandReg(ins, i);
                    if (IsRegisterAllowed(reg)) {
                        // Inserir o callback de análise antes da execução da instrução
                        // Habilitar no cado de uma verificação mais exaustiva. Pode reduzir a acuracia pois a verificação é aplicada sob o endereço de memoria como valor do registrador
                        /*INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegContentAnalysisRoutine,
                            IARG_CONTEXT,
                            IARG_UINT32, reg,
                            IARG_END);*/
                    }
                }

                // Verificar se o operando é do tipo memória
                if (INS_OperandIsMemory(ins, i)) {
                    // Inserir um callback de análise antes da instrução

                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ReadMemoryValue,
                        IARG_THREAD_ID,
                        IARG_PTR, v,
                        IARG_CONTEXT,
                        IARG_PTR, ins,
                        IARG_INST_PTR,
                        IARG_MEMORYREAD_EA,
                        IARG_MEMORYREAD_SIZE,
                        IARG_END);
                }
            }
        }
    }
}

// Fin transferir

VOID InstNtQueryInformationProcess::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    if (RTN_Name(rtn) == "NtQueryInformationProcess") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_PTR, static_cast<void*>(&rtn),
            IARG_CONTEXT,
            IARG_REG_REFERENCE, REG_GAX,
            IARG_RETURN_IP, // Endereço da função chamante
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // ProcessHandle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // ProcessInformationClass
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // ProcessInformation
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ProcessInformationLength
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // ReturnLength
            IARG_END);

        RTN_Close(rtn);
    }
}
