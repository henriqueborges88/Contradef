#include "Instrumentation.h"
#include "HeapTracker.h"
#include "TraceFcnCallM2.h"
#include "DynExecTracker.h"
#include "CommonDef.h"
#include "ContextMonitor.h"
#include "ObsidiumDef.h"
#include "InstructionSequenceDetector.h"

PIN_LOCK lock;


VOID configOutput() {
    using namespace WindowsAPI;
    std::setlocale(LC_ALL, "en_US.UTF-8"); // Definir o local adequado
    SetConsoleOutputCP(CP_UTF8); // Configura o console para UTF-8
    std::wcout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
    std::cout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
}


EXCEPT_HANDLING_RESULT ExceptionHandler(THREADID tid, EXCEPTION_INFO* pExceptInfo, PHYSICAL_CONTEXT* pPhysCtxt, VOID* v) {
    //std::cerr << "Exception caught in thread " << tid << ": " << PIN_ExceptionToString(pExceptInfo) << std::endl;
    //exit(1);
    return EXCEPT_HANDLING_RESULT::EHR_HANDLED;
    // Pode-se modificar o contexto ou apenas registrar o erro
}


int InitInstrumentation() {


    // Iniciar Yara
    initYara();

    // Iniciar o PIN e instrumentação
    PIN_InitSymbols();

    // Configura codificação para saída
    configOutput();

    HeapTracker::Init();


    // Contramedidas genericas
    InitCommonDef();


    // Contramedidas genericas
    InitObsidiumDef();


    // Function Interceptor
    InitFunctionInterceptor();


    // Inst Seq Detector
    if (KnobSeqDetector) {
        InitSeqDetector();
    }


    // Obter o PID do Processo
    string pid = decstr(WindowsAPI::getpid());
    // Log para o interceptador de funções
    string logsName = KnobOutputFile.Value();


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



    // Monitor de troca de Contexto, monitora exceções
    //InitContextMonitor();


    // Controlador de exceções
    PIN_AddInternalExceptionHandler(ExceptionHandler, NULL);


    // [experimental]
    //DynExecTracker::Init();

    PIN_StartProgram();
    return 0;
}