#include "Instrumentation.h"
#include "HeapTracker.h"
#include "SectionTracker.h"
#include "XteaSniffer.h"
#include "TraceFcnCallM2.h"
#include "DynExecTracker.h"

PIN_LOCK lock;

Notifier globalNotifier;
static std::ofstream MainOutFile;

FunctionInterceptor fcnInterceptor; // As estratégias já estão adicionadas no Interceptor

InstructionSequenceDetector seq_detector;
InstructionSequenceDetector disassembly_seq_detector;

static std::wstring GetMainExePath()
{
    wchar_t buf[MAX_PATH];
    DWORD n = WindowsAPI::GetModuleFileNameW(NULL, buf, MAX_PATH); // NULL => módulo principal
    return std::wstring(buf, buf + n);
}

// Função para aguardar no Entry Point
VOID PauseAtEntryPoint(ADDRINT entryAddress) {
    std::cout << "[CONTRADEF] O Contradef pausou no entry point do processo (" << std::hex << entryAddress << ")" << std::endl;
    std::cout << "[CONTRADEF] Anexe o depurador ao processo agora." << std::endl;
    std::cout << "[CONTRADEF] Pressione Enter para continuar a execucao..." << std::endl;

    std::cin.get(); // Aguarda entrada do usuário para continuar
}

static VOID SkipThisCall(ADDRINT nextIp, CONTEXT* c) {
    CONTEXT w; PIN_SaveContext(c, &w);
    PIN_SetContextReg(&w, REG_INST_PTR, nextIp);  // vai para a próxima inst.
    PIN_ExecuteAt(&w);                            // não retorna
}

VOID InstrumentInstructionSeq(INS ins, VOID* v)
{
    seq_detector.InstructionTrace(ins, &seq_detector, onCall);
    disassembly_seq_detector.InstructionTrace(ins, &disassembly_seq_detector, onDisassembly);

    if (INS_IsCall(ins) &&
        INS_OperandCount(ins) > 0 &&
        INS_OperandIsMemory(ins, 0) &&
        INS_MemoryBaseReg(ins) == REG_RBP &&
        INS_MemoryDisplacement(ins) == 0x1ee528)
    {
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(SkipThisCall),
            IARG_ADDRINT, INS_NextAddress(ins),   // próximo RIP
            IARG_CONTEXT,
            IARG_END);
    }
}

VOID InstrumentFunctionInterceptor(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img))
    {
        fcnInterceptor.InitStrategies();

    }

    fcnInterceptor.ExecuteAllStrategies(img, globalNotifier);
}

// Instrumenta o carregamento de imagens
VOID InitPauseAtEntryPoint(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img)) {
        ADDRINT entryPoint = IMG_EntryAddress(img); // IMG_Entry(img);
        std::cout << "[CONTRADEF] Executavel principal carregado: " << IMG_Name(img) << std::endl;
        //std::cout << "[CONTRADEF] Localizando Entry Point: " << std::hex << entryPoint << std::endl;

        PIN_LockClient();
        // Insere o callback para pausar no entry point
        RTN entryRtn = RTN_FindByAddress(entryPoint);
        if (RTN_Valid(entryRtn)) {
            RTN_Open(entryRtn);
            RTN_InsertCall(entryRtn, IPOINT_BEFORE, (AFUNPTR)PauseAtEntryPoint,
                IARG_ADDRINT, entryPoint, // Endereço do Entry Point
                IARG_END);
            RTN_Close(entryRtn);
        }
        else {
            std::cerr << "Falha ao localizar rotina no Entry Point." << std::endl;
        }
        PIN_UnlockClient();

    }
}

// Anti-VM
std::ofstream LogFileTest;
static inline bool Bit64(ADDRINT v, ADDRINT mask) { return (v & mask) != 0; }
static VOID ClearTrapOnPopf(THREADID tid, const CONTEXT* cctx, ADDRINT memea, UINT32 widthBytes)
{
    const ADDRINT cur = PIN_GetContextReg(cctx, REG_RFLAGS);

    const ADDRINT TF = 0x100, RF = 0x10000, AC = 0x40000;
    bool tf_cur = Bit64(cur, TF);
    bool rf_cur = Bit64(cur, RF);
    bool ac_cur = Bit64(cur, AC);

    LogFileTest
        << "[POPF*] EA=" << std::hex << memea
        << " size=" << widthBytes
        << " cur=" << cur << std::dec << "\n"
        << "   TF: cur=" << tf_cur  << "\n"
        << "   RF: cur=" << rf_cur << "\n"
        << "   AC: cur=" << ac_cur  << "\n";
    return;

    // widthBytes: 2 (POPF 16b), 4 (POPF/POPFD 32b), 8 (POPFQ 64b)
    if (widthBytes == 8) {
        UINT64 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 8);
        if (v & 0x100ULL) {
            v &= ~0x100ULL;                 // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 8);
            std::cout << "Aplica do cmd\n";
        }
    }
    else if (widthBytes == 4) {
        UINT32 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 4);
        if (v & 0x100U) {
            v &= ~0x100U;                   // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 4);
            std::cout << "Aplica do cmd\n";

        }
    }
    else if (widthBytes == 2) {
        UINT16 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 2);
        if (v & 0x100U) {
            v &= (UINT16)~0x100U;           // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 2);
            std::cout << "Aplica do cmd\n";

        }
    }
}

static VOID InstrCB(INS ins, VOID*)
{
    const xed_iclass_enum_t iclass = (xed_iclass_enum_t)INS_Opcode(ins);
    if (iclass == XED_ICLASS_POPF ||
        iclass == XED_ICLASS_POPFD ||
        iclass == XED_ICLASS_POPFQ)
    {
        // Endereço efetivo do operando de leitura (o topo da pilha: [SP] antes do pop)
        // e a largura do operando (2/4/8 bytes) para sabermos qual tipo ler/escrever.
        const UINT32 widthBytes = INS_MemoryReadSize(ins); // 2,4,8

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(ClearTrapOnPopf),
            IARG_THREAD_ID,
            IARG_CONST_CONTEXT,
            IARG_MEMORYREAD_EA,
            IARG_MEMORYREAD_SIZE,
            IARG_END);
    }
}
// End Anti-VM

VOID initYara() {
    std::string rules_file = KnobYaraRulesFile.Value();
    if (!rules_file.empty())
    {
        std::string execName = WStringToString(GetMainExePath());
        std::vector<std::string> matched;
        RunYara(rules_file, execName, MainOutFile, matched);

        // Incluir regras na lista de escopos de estratégias
        strategyScope.assign(matched.begin(), matched.end());
    }
}

VOID configOutput() {
    using namespace WindowsAPI;
    std::setlocale(LC_ALL, "en_US.UTF-8"); // Definir o local adequado
    SetConsoleOutputCP(CP_UTF8); // Configura o console para UTF-8
    std::wcout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
    std::cout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
    MainOutFile.imbue(std::locale(""));
    MainOutFile << "\xEF\xBB\xBF"; // Adiciona BOM para indicar UTF-8
}

VOID HandleExecutionCompletedEvent(const EventData* data, void* context) {
    if (data->type != EventData::ExecutionCompleted) {
        return;
    }

    auto executionEvent = static_cast<const ExecutionEventData*>(data);
    auto executionOutputText = executionEvent->executionInformation.outputText;
    MainOutFile << executionOutputText;
    MainOutFile.flush();  // Esvazia o buffer de saída
}

EXCEPT_HANDLING_RESULT ExceptionHandler(THREADID tid, EXCEPTION_INFO* pExceptInfo, PHYSICAL_CONTEXT* pPhysCtxt, VOID* v) {
    std::cerr << "Exception caught in thread " << tid << ": "
        << PIN_ExceptionToString(pExceptInfo) << std::endl;
    //exit(1);
    return EXCEPT_HANDLING_RESULT::EHR_HANDLED;
    // Pode-se modificar o contexto ou apenas registrar o erro
}

// Arquivo de saída para o nosso log.
std::ofstream LogFile;

// Esta é a nossa função de callback. O Pin irá chamá-la antes de entregar
// uma exceção para a aplicação.
#include "pin.H"
#include <iomanip>
#include <sstream>

// ---------------------------------------------
// helpers
// ---------------------------------------------
static const char* ReasonStr(CONTEXT_CHANGE_REASON r) {
    switch (r) {
    case CONTEXT_CHANGE_REASON_SIGNAL:      return "SIGNAL";
    case CONTEXT_CHANGE_REASON_EXCEPTION:   return "EXCEPTION";
    case CONTEXT_CHANGE_REASON_FATALSIGNAL: return "FATALSIGNAL";
    case CONTEXT_CHANGE_REASON_CALLBACK:    return "CALLBACK";
    default:                                return "OTHER";
    }
}

static std::string Hex(ADDRINT v, int width = (int)(sizeof(ADDRINT) * 2)) {
    std::ostringstream oss; oss << "0x"
        << std::hex << std::setw(width) << std::setfill('0') << v
        << std::dec;
    return oss.str();
}

static void DumpFlags(std::ostream& os, ADDRINT rf) {
    const ADDRINT CF = 0x1, PF = 0x4, AF = 0x10, ZF = 0x40, SF = 0x80,
        TF = 0x100, IF = 0x200, DF = 0x400, OF = 0x800,
        RF = 0x10000, AC = 0x40000;
    os << "    RFLAGS=" << Hex(rf) << " ["
        << "CF=" << ((rf & CF) != 0) << " PF=" << ((rf & PF) != 0) << " AF=" << ((rf & AF) != 0)
        << " ZF=" << ((rf & ZF) != 0) << " SF=" << ((rf & SF) != 0) << " TF=" << ((rf & TF) != 0)
        << " IF=" << ((rf & IF) != 0) << " DF=" << ((rf & DF) != 0) << " OF=" << ((rf & OF) != 0)
        << " RF=" << ((rf & RF) != 0) << " AC=" << ((rf & AC) != 0) << "]\n";
}

static void DumpXmm(std::ostream& os, const CONTEXT* c, REG r, const char* name) {
    UINT8 buf[32] = { 0 }; // XMM = 16 bytes, mas 32 não faz mal
    PIN_GetContextRegval(c, r, buf);
    os << "    " << name << "=0x";
    // imprime 16 bytes, mais significativo por último (visualização simples)
    for (int i = 15; i >= 0; --i) os << std::hex << std::setw(2) << std::setfill('0') << (unsigned)buf[i];
    os << std::dec << "\n";
}

static void DumpGprs64(std::ostream& os, const CONTEXT* c, const char* tag) {
    ADDRINT rip = PIN_GetContextReg(c, REG_INST_PTR);
    ADDRINT rsp = PIN_GetContextReg(c, REG_STACK_PTR);
    ADDRINT rflags = PIN_GetContextReg(c, REG_RFLAGS);

    ADDRINT rax = PIN_GetContextReg(c, REG_RAX);
    ADDRINT rbx = PIN_GetContextReg(c, REG_RBX);
    ADDRINT rcx = PIN_GetContextReg(c, REG_RCX);
    ADDRINT rdx = PIN_GetContextReg(c, REG_RDX);
    ADDRINT rsi = PIN_GetContextReg(c, REG_RSI);
    ADDRINT rdi = PIN_GetContextReg(c, REG_RDI);
    ADDRINT rbp = PIN_GetContextReg(c, REG_RBP);
    ADDRINT r8 = PIN_GetContextReg(c, REG_R8);
    ADDRINT r9 = PIN_GetContextReg(c, REG_R9);
    ADDRINT r10 = PIN_GetContextReg(c, REG_R10);
    ADDRINT r11 = PIN_GetContextReg(c, REG_R11);
    ADDRINT r12 = PIN_GetContextReg(c, REG_R12);
    ADDRINT r13 = PIN_GetContextReg(c, REG_R13);
    ADDRINT r14 = PIN_GetContextReg(c, REG_R14);
    ADDRINT r15 = PIN_GetContextReg(c, REG_R15);

    os << "  [" << tag << "] RIP=" << Hex(rip)
        << " RSP=" << Hex(rsp) << "\n"
        << "    RAX=" << Hex(rax) << " RBX=" << Hex(rbx)
        << " RCX=" << Hex(rcx) << " RDX=" << Hex(rdx) << "\n"
        << "    RSI=" << Hex(rsi) << " RDI=" << Hex(rdi)
        << " RBP=" << Hex(rbp) << "  R8=" << Hex(r8) << "  R9=" << Hex(r9) << "\n"
        << "    R10=" << Hex(r10) << " R11=" << Hex(r11)
        << " R12=" << Hex(r12) << " R13=" << Hex(r13)
        << " R14=" << Hex(r14) << " R15=" << Hex(r15) << "\n";
    DumpFlags(os, rflags);

    // Alguns XMMs mais comuns (remova se ficar verboso demais)
    DumpXmm(os, c, REG_XMM0, "XMM0");
    DumpXmm(os, c, REG_XMM1, "XMM1");
    DumpXmm(os, c, REG_XMM2, "XMM2");
    DumpXmm(os, c, REG_XMM3, "XMM3");
}

// ---------------------------------------------
// sua função ajustada
// ---------------------------------------------
static VOID OnContextChange(THREADID tid,
    CONTEXT_CHANGE_REASON reason,
    const CONTEXT* ctxtFrom,
    CONTEXT* ctxtTo,
    INT32 info,
    VOID* v)
{
    std::ostringstream out;
    out << "======================================================\n";
    out << "[CTXCHG] tid=" << tid
        << " reason=" << ReasonStr(reason)
        << " info=0x" << std::hex << info << " (" << std::dec << info << ")\n";

    // Dump dos dois contextos
    if (ctxtFrom) DumpGprs64(out, ctxtFrom, "FROM");
    if (ctxtTo)   DumpGprs64(out, ctxtTo, "TO");

    ADDRINT ipFrom = 0;
    if (ctxtFrom) {
        ipFrom = PIN_GetContextReg(ctxtFrom, REG_INST_PTR);
        if (!IsMainExecutable(ipFrom)) return;
    }

    // Detalhes extras quando for exceção (Windows)
    if (reason == CONTEXT_CHANGE_REASON_EXCEPTION) {
        UINT32 code = info;
        out << "  [EXCEPTION]\n";
        out << "    Code = 0x" << std::hex << code << std::dec <<
        out << "    ExceptIP = " << Hex(ipFrom) << "\n";
    }

    out << "======================================================\n";

    // grava e dá flush
    LogFile << out.str() << std::flush;

}


int InitInstrumentation() {

    // Criando observadores dinamicamente com o contexto nulo
    auto* executionCompleted = new Observer(HandleExecutionCompletedEvent, NULL);
    // Anexa os observers ao notificador
    globalNotifier.Attach(executionCompleted);


    // Obter o PID do Processo
    string pid = decstr(WindowsAPI::getpid());
    // Log para o interceptador de funções
    string logsName = KnobOutputFile.Value();

    initYara();

    // Iniciar o PIN e instrumentação
    PIN_InitSymbols();

    HeapTracker::Init();
    SectionTracker::Init(&globalNotifier);
    
    if (IsStringInStrategyScope("Themida")) {
        std::string excptFileName = logsName + "." + pid + ".test.cdf";
        std::string popFileName = logsName + "." + pid + ".popfq.cdf";
        LogFileTest.open(excptFileName.c_str(), std::ios::out | std::ios::binary);
        INS_AddInstrumentFunction(InstrCB, 0);
    }

    if (KnobFunctionInterceptor) {
        string logfilename = logsName + "." + pid + ".FunctionInterceptor.cdf";
        MainOutFile.open(logfilename.c_str(), std::ios::binary);
        IMG_AddInstrumentFunction(InstrumentFunctionInterceptor, 0);
    }

    if (KnobSeqDetector) {
        // Iniciar detector de sequencia de instruções
        AddSequences(seq_detector, disassembly_seq_detector);
        seq_detector.Initialize();
        disassembly_seq_detector.Initialize();
        INS_AddInstrumentFunction(InstrumentInstructionSeq, 0);
    }

    if (KnobTraceExternalCallTrace) {
        TraceFcnCall::Init(pid, logsName);
        TraceFcnCallM2::Init(pid, logsName);
    }


    if (KnobTraceInstructions) {
        TraceInstructions::Init(pid, logsName);
    }

    if (KnobTraceDisassembly) {
        TraceDisassembly::Init(pid, logsName);
    }

    if (KnobTraceMemory) {
        TraceMemory::Init(pid, logsName);
    }

    if (KnobAllowAttachDebugger) {
        IMG_AddInstrumentFunction(InitPauseAtEntryPoint, nullptr);
    }

    //DynExecTracker::Init();

    // [experimental] habilita o sniffer xtea
    //XteaSniffer::InitXteaSniffer(pid, logsName);

    std::string excptFileName = logsName + "." + pid + ".Exceptions.cdf";
    LogFile.open(excptFileName.c_str(), std::ios::out | std::ios::binary);
    LogFile << "Pintool started. Waiting for exceptions..." << std::endl;
    PIN_AddContextChangeFunction(OnContextChange, 0);


    PIN_AddInternalExceptionHandler(ExceptionHandler, NULL);

    PIN_StartProgram();
    return 0;
}