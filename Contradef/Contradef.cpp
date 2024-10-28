#include "pin.H"

namespace WindowsAPI
{
    #include <Windows.h>
    #include <unistd.h>
}

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
static std::ofstream* TraceOutFile;
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "contradef", "specify file name for output log ");
KNOB<string> KnobYaraRulesFile(KNOB_MODE_WRITEONCE, "pintool", "y", "yaraContradef.yar", "specify file name for YARA rules ");

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
        std::string rules_file = KnobYaraRulesFile.Value(); //"C:\\Users\\henri\\OneDrive\\Escritorio\\Yara Teste\\contradef.yar";
        RunYara(rules_file, execName, OutFile, context);
        
        context.InitStrategies();
    }

    context.ExecuteAllStrategies(img, globalNotifier);
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
}

int main(int argc, char* argv[]) {

    string filename;
    string pid = decstr(WindowsAPI::getpid());
    filename = KnobOutputFile.Value();
    filename += "." + pid + ".log.cdf";
    OutFile.open(filename.c_str());

    // Recuperar o valor do caminho do arquivo YARA
    std::string yaraRulesPath = KnobYaraRulesFile.Value();
    if (yaraRulesPath.empty())
    {
        //PIN_VmFullPath()
        //PIN_ToolFullPath() para obter o yara rules file do mesmo diretorio do PIN tool
        std::cerr << "Por favor, forneça o caminho para o arquivo de regras YARA usando -y." << std::endl;
        return 1;
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

    InitTrace(TraceOutFile, pid);

    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}
