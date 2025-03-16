#include "Instrumentation.h"

PIN_LOCK lock;

Notifier globalNotifier;
static std::ofstream MainOutFile;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "contradef", "specify file name for output logs ");
KNOB<string> KnobYaraRulesFile(KNOB_MODE_WRITEONCE, "pintool", "y", "", "specify file name for YARA rules ");
KNOB<BOOL> KnobAllowAttachDebugger(KNOB_MODE_WRITEONCE, "pintool", "ad", "0", "Allow attach debugger ");

FunctionInterceptor fcnInterceptor; // As estratégias já estão adicionadas no Interceptor

InstructionSequenceDetector seq_detector;
InstructionSequenceDetector disassembly_seq_detector;

bool IsValidWideString(const std::wstring& str) {
    // Verifique primeiro o tamanho mínimo
    if (str.size() < 3) {
        return false;
    }

    for (wchar_t ch : str) {
        // Permitir apenas caracteres ASCII alfanuméricos e os permitidos extras
        if (!((ch >= L'0' && ch <= L'9') || // Números
            (ch >= L'A' && ch <= L'Z') || // Letras maiúsculas
            (ch >= L'a' && ch <= L'z') || // Letras minúsculas
            ch == L'.' || ch == L'-' || ch == L'_' ||
            ch == L':' || ch == L'\\' || ch == L'/')) {
            return false; // Caractere inválido encontrado
        }
    }

    return true; // Todos os caracteres são válidos
}


bool IsValidString(const std::string& str) {
    // Verifique primeiro o tamanho mínimo
    if (str.size() < 3) {
        return false;
    }

    for (char ch : str) {
        // Permitir apenas caracteres ASCII alfanuméricos e os extras permitidos
        if (!((ch >= '0' && ch <= '9') || // Números
            (ch >= 'A' && ch <= 'Z') || // Letras maiúsculas
            (ch >= 'a' && ch <= 'z') || // Letras minúsculas
            ch == '.' || ch == '-' || ch == '_' ||
            ch == ':' || ch == '\\' || ch == '/')) {
            return false; // Caractere inválido encontrado
        }
    }

    return true; // Todos os caracteres são válidos
}

std::wstring aCopyLPCWSTR(ADDRINT addr) {
    const size_t MAX_STRING_LENGTH = 800; // Defina o tamanho máximo da string
    wchar_t buffer[MAX_STRING_LENGTH];    // Buffer para armazenar a string copiada
    memset(buffer, 0, sizeof(buffer));    // Inicializa o buffer com zeros

    size_t copied = 0;                    // Contador de caracteres copiados

    for (size_t i = 0; i < MAX_STRING_LENGTH - 1; i++) {
        // Use PIN_SafeCopy para copiar um único caractere wide por vez (2 bytes em UTF-16/UCS-2)
        size_t result = PIN_SafeCopy(&buffer[i], reinterpret_cast<const void*>(addr + i * sizeof(wchar_t)), sizeof(wchar_t));

        // Se a cópia falhar ou encontrar o terminador nulo, finalize
        if (result != sizeof(wchar_t) || buffer[i] == L'\0') {
            break;
        }

        copied++;
    }

    // Verifica se o tamanho máximo foi excedido
    if (copied == MAX_STRING_LENGTH - 1) {
        //std::wcerr << L"Warning: Wide string copy reached the maximum limit." << std::endl;
    }

    return std::wstring(buffer); // Retorna a string wide copiada como um std::wstring
}

std::string aCopyLPCSTR(ADDRINT addr) {
    const size_t MAX_STRING_LENGTH = 800; // Defina o tamanho máximo da string
    char buffer[MAX_STRING_LENGTH];       // Buffer para armazenar a string copiada
    memset(buffer, 0, sizeof(buffer));    // Inicializa o buffer com zeros

    size_t copied = 0;                    // Contador de bytes copiados

    for (size_t i = 0; i < MAX_STRING_LENGTH - 1; i++) {
        // Use PIN_SafeCopy para copiar um único caractere por vez
        size_t result = PIN_SafeCopy(&buffer[i], reinterpret_cast<const void*>(addr + i), 1);

        // Se a cópia falhar ou encontrar o terminador nulo, finalize
        if (result == 0 || buffer[i] == '\0') {
            break;
        }

        copied++;
    }

    // Verifica se o tamanho máximo foi excedido
    if (copied == MAX_STRING_LENGTH - 1) {
        //std::cerr << "Warning: String copy reached the maximum limit." << std::endl;
    }

    return std::string(buffer); // Retorna a string copiada como um std::string
}


// Função para aguardar no Entry Point
VOID PauseAtEntryPoint(ADDRINT entryAddress) {
    std::cout << "[CONTRADEF] O Contradef pausou no entry point do processo (" << std::hex << entryAddress << ")" << std::endl;
    std::cout << "[CONTRADEF] Anexe o depurador ao processo agora." << std::endl;
    std::cout << "[CONTRADEF] Pressione Enter para continuar a execucao..." << std::endl;

    std::cin.get(); // Aguarda entrada do usuário para continuar
}


VOID GetSectionInfo(IMG img, std::ofstream& MainOutFile)
{
    MainOutFile << std::endl << "[+] Informação de seções..." << std::endl;
    MainOutFile << "    Nome da imagem: " << std::string(IMG_Name(img)) << std::endl;

    // Índice da seção
    int index = 0;

    // Itera sobre as seções da imagem
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec), ++index)
    {
        std::string secName = SEC_Name(sec);
        secName = secName.empty() ? "[VAZIO]" : secName;
        ADDRINT secAddr = SEC_Address(sec);
        UINT64 secSize = SEC_Size(sec);

        MainOutFile << "    Seção [" << index << "]" << std::endl;
        MainOutFile << "        Nome da seção: " << secName << std::endl;
        MainOutFile << "        Endereço: 0x" << std::hex << static_cast<uint64_t>(secAddr) << std::dec << std::endl;
        MainOutFile << "        Tamanho: " << static_cast<uint64_t>(secSize) << " bytes" << std::endl;

        MainOutFile << "        Características: " << std::endl;
        if (SEC_IsExecutable(sec))
        {
            MainOutFile << "            A seção é executável." << std::endl;
        }
        if (SEC_IsReadable(sec))
        {
            MainOutFile << "            A seção é legível." << std::endl;
        }
        if (SEC_IsWriteable(sec))
        {
            MainOutFile << "            A seção é gravável." << std::endl;
        }
    }

    MainOutFile << "[*] Concluído" << std::endl << std::endl;

    MainOutFile.flush();  // Esvazia o buffer de saída
}


///////////////////

// Test error CB
EXCEPT_HANDLING_RESULT errCB (THREADID tid, EXCEPTION_INFO* pExceptInfo, PHYSICAL_CONTEXT* pPhysCtxt, VOID* v)
{
    return EXCEPT_HANDLING_RESULT::EHR_HANDLED;
}

VOID aEmitRead(THREADID threadid, ADDRINT instAddress, std::string* insstr, VOID* ea, UINT32 size)
{
    ADDRINT address = reinterpret_cast<ADDRINT>(ea);
    return;

    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(ea)))
    {
        std::wstring wstr = aCopyLPCWSTR(address);
        if (IsValidWideString(wstr)) {
            std::cout << "Read from address: " << std::hex << address << ", size: " << size << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << *insstr << std::endl;
            wprintf(L" ** Read WString --> %ls <-\n", wstr);
        }
        
        std::string str = aCopyLPCSTR(address);
        if (IsValidString(str)) {
            std::cout << "Read from address: " << std::hex << address << ", size: " << size << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << *insstr << std::endl;
            std::cout << " ** Read String --> " << str << std::endl;
        }
                
    }

}


//Remover depois de implementar o memory trace
//VOID CaptureWriteEa(THREADID threadid, ADDRINT instAddress, VOID* addr) {
//    ADDRINT address = reinterpret_cast<ADDRINT>(addr) - 0x0;
//    return;
//
//    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(addr)))
//    {
//        std::wstring wstr = aCopyLPCWSTR(address);
//        //if (true) { // (wstr.find(L"Fracture") != std::wstring::npos) {
//        //    std::cout << "Wstr achado:  " << instAddress << std::endl;
//        //    std::cout << "Read from address: " << std::hex << address << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << std::endl;
//        //    wprintf(L" ** RETURN WString --> %ls <-\n", wstr);
//        //}
//        if (IsValidWideString(wstr)) {
//            std::cout << "Write from address: " << std::hex << address << " bytes" << " -> " << std::hex << instAddress << std::dec << std::endl;
//            wprintf(L" ** Write WString --> %ls <-\n", wstr);
//        }
//
//        std::string str = aCopyLPCSTR(address);
//        //if (true) { //(str.find("Fracture") != std::string::npos) {
//        //    std::cout << "Str achado:  " << instAddress << std::endl;
//        //    std::cout << "Read from address: " << std::hex << address  << " -> " << std::hex << instAddress << std::dec << " > " << std::endl;
//        //    std::cout << " ** RETURN String --> " << str << std::endl;
//        //}
//         if (IsValidString(str)) {
//            std::cout << "Write from address: " << std::hex << address << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << std::endl;
//            std::cout << " ** Write String --> " << str << std::endl;
//        }
//
//    }
//}


//Remover depois de implementar o memory trace
//VOID aMemoryTrace(INS ins)
//{
//
//    if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
//    {
//        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CaptureWriteEa), IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYWRITE_EA, IARG_END);
//
//       /* if (INS_IsValidForIpointAfter(ins))
//        {
//            INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
//        }
//        if (INS_IsValidForIpointTakenBranch(ins))
//        {
//            INS_InsertPredicatedCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE,
//                IARG_END);
//        }*/
//    }
//    string memString = INS_Disassemble(ins);
//    if (INS_HasMemoryRead2(ins) && INS_IsStandardMemop(ins))
//    {
//        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aEmitRead), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new std::string(memString), IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
//            IARG_END);
//    }
//
//    if (INS_IsMemoryRead(ins) && !INS_IsPrefetch(ins) && INS_IsStandardMemop(ins))
//    {
//        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aEmitRead), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new std::string(memString), IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
//            IARG_END);
//    }
//}



VOID RecordLEA(VOID* ip, std::string* insstr, REG reg, ADDRINT regValue) {

    ADDRINT result = regValue;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        std::wstring wstr = aCopyLPCWSTR(result);
        if (IsValidWideString(wstr)) {
            std::cout << "Read from address: " << std::hex << result << " bytes" << " -> " << std::hex << ip << std::dec << " > " << *insstr << std::endl;
            wprintf(L" ** RETURN WString --> %ls <-\n", wstr);
        }

        std::string str = aCopyLPCSTR(result);
        if (IsValidString(str)) {
            std::cout << "Read from address: " << std::hex << result << " bytes" << " -> " << std::hex << ip << std::dec << " > " << *insstr << std::endl;
            std::cout << " ** RETURN String --> " << str << std::endl;
        }

    }
}

//Passar para o detector de seq de inst
VOID TraceInstSeq(INS ins, VOID* v)
{
    seq_detector.InstructionTrace(ins, &seq_detector, onCall);
    disassembly_seq_detector.InstructionTrace(ins, &disassembly_seq_detector, onDisassembly);
}


VOID InstrumentFunctionInterceptor(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img))
    {
        GetSectionInfo(img, MainOutFile);

        std::string execName = IMG_Name(img);
        std::string rules_file = KnobYaraRulesFile.Value();

        std::string yaraRulesPath = KnobYaraRulesFile.Value();
        if (!yaraRulesPath.empty())
        {
            std::vector<std::string> matched;
            RunYara(rules_file, execName, MainOutFile, matched);
            fcnInterceptor.scanScope.assign(matched.begin(), matched.end());
        }

        fcnInterceptor.InitStrategies();
    }

    fcnInterceptor.ExecuteAllStrategies(img, globalNotifier);
}

// Instrumenta o carregamento de imagens
VOID InstrumentImageLoad(IMG img, VOID* v) {
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

int InitInstrumentation()
{
    testSequences(seq_detector, disassembly_seq_detector);

    // Recuperar o valor do caminho do arquivo YARA
    std::string yaraRulesPath = KnobYaraRulesFile.Value();
    if (yaraRulesPath.empty())
    {
        //PIN_VmFullPath()
        //PIN_ToolFullPath() para obter o yara rules file do mesmo diretorio do PIN tool
        //std::cerr << "Por favor, forneça o caminho para o arquivo de regras YARA usando -y." << std::endl;
        //return 1;
    }


    // Criando observadores dinamicamente com o contexto nulo
    auto* executionCompleted = new Observer(HandleExecutionCompletedEvent, NULL);
    // Anexa os observers ao notificador
    globalNotifier.Attach(executionCompleted);


    // Obter o PID do Processo
    string pid = decstr(WindowsAPI::getpid());


    // Log Principal
    string logsName = KnobOutputFile.Value();
    string logfilename = logsName + "." + pid + ".log.cdf";
    MainOutFile.open(logfilename.c_str(), std::ios::binary);


    // Iniciar detector de sequencia de instruções
    seq_detector.Initialize();
    disassembly_seq_detector.Initialize();


    // Iniciar o PIN e instrumentação
    PIN_InitLock(&lock);
    PIN_InitSymbols();

    IMG_AddInstrumentFunction(InstrumentFunctionInterceptor, 0);

    //INS_AddInstrumentFunction(TraceInstSeq, 0);

    //TraceInstructions::InitTrace(pid, logsName);

    //TraceDisassembly::InitTraceDisassembly(pid, logsName);

    //TraceMemory::InitMemoryTrace(pid, logsName);

    //TraceFcnCall::InitFcnCallTrace(pid, logsName);


    if (KnobAllowAttachDebugger)
    {
        //IMG_AddInstrumentFunction(InstrumentImageLoad, nullptr);
    }


    PIN_StartProgram();
    return 0;
}