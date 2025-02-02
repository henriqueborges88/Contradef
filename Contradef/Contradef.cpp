#include "pin.H"

#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include <deque>
#include <map>
#include "InstrumentationContext.h"
#include "Notifier.h"
#include <cstdint>
#include "YaraContradef.h"
#include "NtStructures.h"
#include "PinDebugTrace.h"
#include "Instrumentation.h"

using std::string;
using std::cerr;
using std::cout;
using std::wcerr;
using std::endl;
using std::flush;
using std::set;
using std::wstring;

PIN_LOCK lock;
static std::ofstream OutFile;
static std::ofstream ExternalCallTraceOutFile;
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "contradef", "specify file name for output log ");
KNOB<string> KnobYaraRulesFile(KNOB_MODE_WRITEONCE, "pintool", "y", "", "specify file name for YARA rules ");
KNOB<BOOL> KnobAllowAttachDebugger(KNOB_MODE_WRITEONCE, "pintool", "ad", "0", "Allow attach debugger ");
KNOB<BOOL> KnobSaveExternalCallTrace(KNOB_MODE_WRITEONCE, "pintool", "et", "1", "Save external call trace file ");

InstrumentationContext context; // As estratégias já estão adicionadas no contexto

Notifier globalNotifier;

INT32 Usage()
{
    cerr << "Esta ferramenta PIN fornece informação sobre possíveis comportamentos evasivos (antianálise) presentes em executáveis\n"
        "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

VOID InstrumentFunctions(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img))
    {
        GetSectionInfo(img, OutFile);

        std::string execName = IMG_Name(img);
        std::string rules_file = KnobYaraRulesFile.Value();
        
        std::string yaraRulesPath = KnobYaraRulesFile.Value();
        if (!yaraRulesPath.empty())
        {
            RunYara(rules_file, execName, OutFile, context);
        }
        
        context.InitStrategies();
    }

    context.ExecuteAllStrategies(img, globalNotifier);
}

VOID configOutput() {
    using namespace WindowsAPI;
    std::setlocale(LC_ALL, "en_US.UTF-8"); // Definir o local adequado
    SetConsoleOutputCP(CP_UTF8); // Configura o console para UTF-8
    std::wcout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
    std::cout.imbue(std::locale("")); // Configura a saída wide para usar a codificação local
    OutFile.imbue(std::locale(""));
    OutFile << "\xEF\xBB\xBF"; // Adiciona BOM para indicar UTF-8
}

VOID HandleExecutionCompletedEvent(const EventData* data, void* context) {
    if (data->type != EventData::ExecutionCompleted) {
        return;
    }

    auto executionEvent = static_cast<const ExecutionEventData*>(data);
    auto executionOutputText = executionEvent->executionInformation.outputText;
    OutFile << executionOutputText;
    OutFile.flush();  // Esvazia o buffer de saída
}


VOID Fini(int, VOID* v)
{
    OutFile.close();
    //TraceOutFile->close();

    if (KnobSaveExternalCallTrace)
    {
        saveAllExternalCalls(ExternalCallTraceOutFile);
    }

    ExternalCallTraceOutFile.close();
}

// Instrumenta o carregamento de imagens
VOID ImageLoad(IMG img, VOID* v) {
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

int main(int argc, char* argv[]) {

    //configOutput();

    string filename;
    string pid = decstr(WindowsAPI::getpid());
    filename = KnobOutputFile.Value();

    string logfilename = filename + "." + pid + ".log.cdf";
    OutFile.open(logfilename.c_str(), std::ios::binary);

    // Recuperar o valor do caminho do arquivo YARA
    std::string yaraRulesPath = KnobYaraRulesFile.Value();
    if (yaraRulesPath.empty())
    {
        //PIN_VmFullPath()
        //PIN_ToolFullPath() para obter o yara rules file do mesmo diretorio do PIN tool
        //std::cerr << "Por favor, forneça o caminho para o arquivo de regras YARA usando -y." << std::endl;
        //return 1;
    }

    if (KnobSaveExternalCallTrace)
    {
        string logfilename = filename + "." + pid + ".externalcalltrace.cdf";
        ExternalCallTraceOutFile.open(logfilename.c_str(), std::ios::binary);
    }

    PIN_InitSymbols();

    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    // Criando observadores dinamicamente com o contexto nulo
    auto* executionCompleted = new Observer(HandleExecutionCompletedEvent, NULL);

    // Anexa os observers ao notificador
    globalNotifier.Attach(executionCompleted);
    
    PIN_InitLock(&lock);

    PIN_InitSymbols();

    IMG_AddInstrumentFunction(InstrumentFunctions, 0);

    InitInstrumentation(pid);

    PIN_AddFiniFunction(Fini, 0);

    if (KnobAllowAttachDebugger)
    {
        IMG_AddInstrumentFunction(ImageLoad, nullptr);
    }
    

    
    PIN_StartProgram();

    return 0;
}
