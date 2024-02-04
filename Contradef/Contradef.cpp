#include "pin.H"
#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include <unistd.h>
#include <deque>
#include <map>
#include "InstrumentationContext.h"
#include "Notifier.h"

using std::string;
using std::cerr;
using std::cout;
using std::wcerr;
using std::endl;
using std::flush;
using std::set;
using std::wstring;



const size_t MAX_TRACE_SIZE = 40; // Número de rotinas a rastrear
std::queue<InstuctionInformation> instructionTrace;
std::map<THREADID, std::deque<CallRtnInformation>> threadTraces;
PIN_LOCK lock;
std::ofstream OutFile;
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "contradef.out", "specify file name for insmix profile ");
InstrumentationContext context; // As estratégias já estão adicionadas no contexto
std::queue<string*> instructionLog; // considerar este metodo para imprimir todas as strings no final

Notifier globalNotifier;

/*
EnumProcessModules/EnumProcessModulesEx: Essas funções podem ser usadas para enumerar os módulos carregados em um processo. Isso é útil para detectar DLLs suspeitas ou inesperadas que podem ser injetadas por instrumentadores binários.

GetModuleHandle/GetModuleFileName: Essas funções ajudam a obter handles e nomes de arquivos de módulos específicos. Você pode usá-las para verificar se um módulo conhecido de um instrumentador DBI, como o dynamorio.dll (usado pelo DynamoRIO) ou pinvm.dll (usado pelo Intel PIN), está carregado em um processo.

VirtualQuery/VirtualProtect: Estas funções permitem inspecionar e modificar as permissões de páginas de memória. Instrumentadores DBI frequentemente modificam permissões de memória para injetar código ou realizar a instrumentação. GetModuleHandle ou EnumProcessModules podem tentar ver se VirtualQuery/VirtualProtect estão carregadas

CreateToolhelp32Snapshot/Module32First/Module32Next: Estas funções são usadas para tirar um 'snapshot' dos processos e módulos em execução no sistema. Eles podem ser usados para detectar alterações suspeitas no espaço de endereço de um processo.

Debug API (como DebugActiveProcess): A API de Debug do Windows pode ser utilizada para anexar a um processo e monitorar suas atividades, o que pode revelar a presença de DBIs.

Event Tracing for Windows (ETW): O ETW fornece um mecanismo de rastreamento de eventos de alto desempenho. Você pode usar o ETW para monitorar eventos do sistema e de aplicativos que podem indicar a presença de instrumentação binária.

NtQueryInformationProcess: Esta função de baixo nível pode ser usada para obter informações detalhadas sobre um processo, incluindo a presença de hooks ou alterações suspeitas no espaço de endereço.

System Service Descriptor Table (SSDT) Hooking: Verificar alterações no SSDT, pois os DBIs podem modificar esta tabela para interceptar chamadas de sistema.

    
            "module": "KERNEL32.DLL",
            "function": "GetProcAddress",
            "address": "0x7ffc3e4c3c10"
     
            "module": "KERNEL32.DLL",
            "function": "GetCurrentProcess",
            "address": "0x7ffc3e4d0160"

        Themida
       
            "module": "KERNEL32.DLL",
            "function": "GetCurrentThreadId",
            "address": "0x7ffc3e4b2750"
        
            "module": "KERNEL32.DLL",
            "function": "GetCommandLineA",
            "address": "0x7ffc3e4c8ad0"
        
            "module": "KERNEL32.DLL",
            "function": "LoadLibraryExW",
            "address": "0x7ffc3e4c3d90"

        ZwQueryInformationThread
        ZwOpenProcess
        NtOpenProcess
        kernelbase.DebugBreak
        kernelbase.IsDBCSLeadByte
        kernelbase.GetEnvironmentStringsW <- envs do DBI?
        kernelbase.ExpandEnvironmentStringsW
        kernel32.ProcessIdToSessionId
        kernel32.GetProcessTimes
        kernel32.GetCurrentThreadId
        kernel32.GetCurrentProcess
        kernel32.GetCurrentProcessId

        obsedium:
        Export OptionalHeader.AddressOfEntryPoint <_ verificar isso
        shell32.ShellAboutW
        user32.CharNextA
        advapi32.RegisterEventSourceA

        EtwLogTraceEvent de ntdll.dll
    */

INT32 Usage()
{
    cerr << "Esta ferramenta PIN fornece informação sobre possíveis comportamentos evasivos (antianálise) presentes em executáveis\n"
        "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

VOID InstrumentFunctions(IMG img, VOID* v) {
    context.ExecuteAllStrategies(img, OutFile, globalNotifier);
}

std::map<ADDRINT, InstuctionInformation> globalInstructionInfoMap;

VOID BeforeIndirectCall(ADDRINT instrAddr, ADDRINT targetAddr) {
    PIN_LockClient();
        RTN targetRtn = RTN_FindByAddress(targetAddr);
        if (RTN_Valid(targetRtn)) {
            auto insIt = globalInstructionInfoMap.find(instrAddr);
            if (insIt != globalInstructionInfoMap.end()) {
                InstuctionInformation instructionInfo = insIt->second;
                std::string targetName = RTN_Name(targetRtn);
                instructionInfo.disassembledInstruction += "                   ; " + targetName;
                if (instructionTrace.size() >= MAX_TRACE_SIZE) {
                    instructionTrace.pop(); // Remove a instrução mais antiga
                }
                instructionTrace.push(instructionInfo); // Adiciona a nova instrução

                // Cria evento
                InstructionEventData instructionEvent(instructionInfo);
                // Notifica os observers
                globalNotifier.NotifyAll(&instructionEvent);
                
                globalInstructionInfoMap.erase(insIt);
            }
        }
    PIN_UnlockClient();
}

VOID Instruction(INS ins, VOID* v) {
    ADDRINT address = INS_Address(ins);
    std::string disassembledInstruction = INS_Disassemble(ins);

    PIN_LockClient();
    IMG img = IMG_FindByAddress(address);
    PIN_UnlockClient();
    if (IMG_Valid(img)) {
        if (!IMG_IsMainExecutable(img)) {
            // Se a leitura não ocorre na imagem principal (executavel). Alguns protetores podem carregar funções em modulos diferentes ao principal, se for o caso, desativar o return
            return;
        }
    }

    InstuctionInformation instructionInfo = { &instructionTrace, address, disassembledInstruction };

    if (INS_IsCall(ins)) {
        if (!INS_IsDirectBranchOrCall(ins)) {
            ADDRINT address = INS_Address(ins);
            globalInstructionInfoMap[address] = instructionInfo; // Armazena o objeto INS com base no endereço da instrução para ser recuperado no callback

            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BeforeIndirectCall,
                IARG_INST_PTR,
                IARG_BRANCH_TARGET_ADDR,
                IARG_END);
            // deixa o CB BeforeIndirectCall registrar a instruçao, return para não continuar
            return;
        } 
        else {
            ADDRINT targetAddr = INS_DirectBranchOrCallTargetAddress(ins);
            RTN targetRtn = RTN_FindByAddress(targetAddr);
            if (RTN_Valid(targetRtn)) {
                std::string targetName = RTN_Name(targetRtn);
                instructionInfo.disassembledInstruction += "                   ; " + targetName;
            }
        }
    }

    if (instructionTrace.size() >= MAX_TRACE_SIZE) {
        instructionTrace.pop(); // Remove a instrução mais antiga
    }
    instructionTrace.push(instructionInfo); // Adiciona a nova instrução

    // Cria evento
    InstructionEventData instructionEvent(instructionInfo);
    // Notifica os observers
    globalNotifier.NotifyAll(&instructionEvent);

}

VOID BeforeCall(THREADID threadid, ADDRINT funcAddress, ADDRINT returnAddress) {

    PIN_GetLock(&lock, threadid);
    std::string rtnCallerName = RTN_FindNameByAddress(returnAddress);
    if (rtnCallerName.empty()) {
        rtnCallerName = "-";
    }

    // Para registrar as chamadas (trace) procedentes apenas do processo principal
    PIN_LockClient();
    IMG imgCaller = IMG_FindByAddress(returnAddress);
    IMG imgRtn = IMG_FindByAddress(funcAddress);
    PIN_UnlockClient();
    if (IMG_Valid(imgCaller)) {
        if (!IMG_IsMainExecutable(imgCaller)) {
            PIN_ReleaseLock(&lock);
            // Se a leitura não ocorre na imagem principal (executavel). Alguns protetores podem carregar funções em modulos diferentes ao principal, se for o caso, desativar o return
            return;
        }
    }
    
    std::string rtnName = RTN_FindNameByAddress(funcAddress);
    
    auto& trace = threadTraces[threadid]; // Obtem o deque específico do thread
    if (trace.size() >= MAX_TRACE_SIZE) {
        trace.pop_front();
    }

    CallRtnInformation callRtnInformation = { funcAddress, rtnName, IMG_Name(imgRtn), rtnCallerName };
    trace.push_back(callRtnInformation);

    CallTraceInformation callInfo = { &threadTraces, callRtnInformation };
    // Cria evento
    TraceEventData traceEvent(callInfo);
    // Notifica os observers
    globalNotifier.NotifyAll(&traceEvent);

    PIN_ReleaseLock(&lock);

}

VOID Routine(RTN rtn, VOID* v) {
    RTN_Open(rtn);

    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)BeforeCall,
        IARG_THREAD_ID,
        IARG_ADDRINT, RTN_Address(rtn),
        IARG_RETURN_IP, // Endereço da função chamante
        IARG_END);

    RTN_Close(rtn);
}


int main(int argc, char* argv[]) {

    string filename;
    filename = KnobOutputFile.Value();
    filename += "." + decstr(getpid());
    OutFile.open(filename.c_str());

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }
    
    PIN_InitLock(&lock);

    PIN_InitSymbols();

    INS_AddInstrumentFunction(Instruction, 0);

    RTN_AddInstrumentFunction(Routine, 0);

    IMG_AddInstrumentFunction(InstrumentFunctions, 0);

    PIN_StartProgram();

    OutFile.close();
    return 0;
}