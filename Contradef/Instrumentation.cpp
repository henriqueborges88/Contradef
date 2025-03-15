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

    // Tecnicas evasivas obsiium 1.8.5 Antidbi
    std::vector<std::vector<std::string>> sequence;
    sequence.push_back(CreateInstVector("jmp 0x7ff6f499d6e1"));
    sequence.push_back(CreateInstVector("test word ptr [r?i+0xa], 0x2"));
    sequence.push_back(CreateInstVector("jmp 0x7ff6f499d6eb", "jnb 0x7ff6f499d6eb", "jno 0x7ff6f499d6eb", "jb 0x7ff7bf17d3eb", "jo 0x7ff6cafbeb87"));
    sequence.push_back(CreateInstVector("jz 0x7ff6f499d2da"));

    //seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback, 34, 34);
    seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback); // TODAS



    // Instrução para ver as funções que será chamadas
    std::vector<std::vector<std::string>> sequence_call_fcn;
    sequence_call_fcn.push_back(CreateInstVector("add rsp, 0x?8"));
    sequence_call_fcn.push_back(CreateInstVector("pop r13"));
    sequence_call_fcn.push_back(CreateInstVector("pop r12"));
    sequence_call_fcn.push_back(CreateInstVector("jmp r10"));
    //seq_detector.AddInstructionSequence(sequence_call_fcn, SequenceMatchCallFcnCallback);


    // Função que escolhe o texto de erro que aparece na tela na tela
    std::vector<std::vector<std::string>> sequence_print_fcn;
    sequence_print_fcn.push_back(CreateInstVector("movzx r10d, word ptr [rax]")); // <- 0x7ff6a5335780
    sequence_print_fcn.push_back(CreateInstVector("test r10d, r10d"));
    sequence_print_fcn.push_back(CreateInstVector("jz 0x7ff6a533579c"));
    sequence_print_fcn.push_back(CreateInstVector("cmp r10b, cl"));
    sequence_print_fcn.push_back(CreateInstVector("jz 0x7ff6a533579e")); // <- se é a string procurada ele salta
    sequence_print_fcn.push_back(CreateInstVector("jnbe 0x7ff6a533579c"));
    sequence_print_fcn.push_back(CreateInstVector("movzx r11d, word ptr [rax+0x2]"));
    sequence_print_fcn.push_back(CreateInstVector("lea rax, ptr [rax+r11*2+0x4]"));
    sequence_print_fcn.push_back(CreateInstVector("jmp 0x7ff6a5335780")); // <- volta para: movzx r10d, word ptr [rax]
    //seq_detector.AddInstructionSequence(sequence_print_fcn, SequenceMatchReadStringForCallback);



    //// Função que varifica o texto para os nomes de funçoes - FUNCIONANDO
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("jz 0x7ff6d6ed3e4b"));
    //sequence_read_string_fcn.push_back(CreateInstVector("xor eax, eax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("xor r8, r8"));
    //sequence_read_string_fcn.push_back(CreateInstVector("dec eax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov r8b, byte ptr [rcx]"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringFcnCallback);

    // Contem a string "=ExitCode=00000000"
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("jnz 0x7ff6d6eabc9c"));
    //sequence_read_string_fcn.push_back(CreateInstVector("lea rbp, ptr [rbp+rax*2]"));
    //sequence_read_string_fcn.push_back(CreateInstVector("add rbp, 0x2"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp word ptr [rbp], si"));
    //sequence_read_string_fcn.push_back(CreateInstVector("jnz 0x7ff6d6eabc98"));
    //sequence_read_string_fcn.push_back(CreateInstVector("or rax, 0xffffffffffffffff"));
    //sequence_read_string_fcn.push_back(CreateInstVector("inc rax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp word ptr [rbp+rax*2], si"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringEnvsCallback);






    //Algumas strings, IMPORTNTE, segue a função GetModuleFileNameW, e precede a função openFile e closeHandle
    std::vector<std::vector<std::string>> sequence_read_string_fcn;
    sequence_read_string_fcn.push_back(CreateInstVector("jmp 0x7ff6d6ecf9d6"));
    sequence_read_string_fcn.push_back(CreateInstVector("jmp 0x7ff6d6ecf37b"));
    sequence_read_string_fcn.push_back(CreateInstVector("movzx eax, byte ptr [rcx]"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringStrings1Callback);




    std::vector<std::vector<std::string>> read_string_cmp;
    read_string_cmp.push_back(CreateInstVector("or rax, 0xffffffffffffffff"));
    read_string_cmp.push_back(CreateInstVector("inc rax"));
    read_string_cmp.push_back(CreateInstVector("cmp word ptr [rbp+rax*2], si"));
    read_string_cmp.push_back(CreateInstVector("jnz 0x7ff7e1c5bc9c"));
    read_string_cmp.push_back(CreateInstVector("inc rax"));
    read_string_cmp.push_back(CreateInstVector("cmp word ptr [rbp+rax*2], si"));
    //seq_detector.AddInstructionSequence(read_string_cmp, SequenceMatchReadStringCmpCallback);



    std::vector<std::vector<std::string>> read_string_cmp2;
    read_string_cmp2.push_back(CreateInstVector("jnz 0x7ff7e1c5bc9c"));
    read_string_cmp2.push_back(CreateInstVector("lea rbp, ptr [rbp+rax*2]"));
    read_string_cmp2.push_back(CreateInstVector("add rbp, 0x2"));
    read_string_cmp2.push_back(CreateInstVector("cmp word ptr [rbp], si"));
    //seq_detector.AddInstructionSequence(read_string_cmp2, SequenceMatchReadStringCmp2Callback);


    std::vector<std::vector<std::string>> read_string_cmp3;
    read_string_cmp3.push_back(CreateInstVector("mov rdi, rax"));
    read_string_cmp3.push_back(CreateInstVector("test rax, rax"));
    read_string_cmp3.push_back(CreateInstVector("jnz 0x7ff7e1c5bd0c"));
    read_string_cmp3.push_back(CreateInstVector("mov qword ptr [rsp+0x38], rsi"));
    read_string_cmp3.push_back(CreateInstVector("mov r9d, ebp"));
    read_string_cmp3.push_back(CreateInstVector("mov qword ptr [rsp+0x30], rsi"));
    read_string_cmp3.push_back(CreateInstVector("mov r8, rbx"));
    read_string_cmp3.push_back(CreateInstVector("mov dword ptr [rsp+0x28], r14d"));
    read_string_cmp3.push_back(CreateInstVector("xor edx, edx"));
    read_string_cmp3.push_back(CreateInstVector("xor ecx, ecx"));
    read_string_cmp3.push_back(CreateInstVector("mov qword ptr [rsp+0x20], rdi"));
    //seq_detector.AddInstructionSequence(read_string_cmp3, SequenceMatchReadStringCmp3Callback);


    // IMPORTANTE - Contem o nome de alguns depuradores
    std::vector<std::vector<std::string>> read_string_cmp40;
    read_string_cmp40.push_back(CreateInstVector("jmp 0x7ff625001cd1"));
    read_string_cmp40.push_back(CreateInstVector("jmp 0x7ff625001cd6"));
    read_string_cmp40.push_back(CreateInstVector("mov al, byte ptr [r10+rcx*1+0x1]"));
    read_string_cmp40.push_back(CreateInstVector("jmp 0x7ff625001cde"));
    //seq_detector.AddInstructionSequence(read_string_cmp40, SequenceMatchReadStringCmp40Callback);


    // IMPORTANTE - Contem os valores a serem comparados com os nomes de alguns depuradores
    std::vector<std::vector<std::string>> read_string_cmp41;
    read_string_cmp41.push_back(CreateInstVector("mov al, byte ptr [r10+rcx*1+0x1]"));
    read_string_cmp41.push_back(CreateInstVector("jmp 0x7ff625001cde"));
    read_string_cmp41.push_back(CreateInstVector("test al, al"));
    read_string_cmp41.push_back(CreateInstVector("jno 0x7ff625001ce4"));
    read_string_cmp41.push_back(CreateInstVector("jz 0x7ff625001d46"));
    read_string_cmp41.push_back(CreateInstVector("jmp 0x7ff625001ced"));
    read_string_cmp41.push_back(CreateInstVector("cmp ax, word ptr [rbp+rcx*2]"));
    //seq_detector.AddInstructionSequence(read_string_cmp41, SequenceMatchReadStringCmp41Callback);


    // Tecnica antidebug, verificada
    std::vector<std::vector<std::string>> ad_same_target;
    ad_same_target.push_back(CreateInstVector("jz 0x7ff62f10fe35"));
    ad_same_target.push_back(CreateInstVector("jnz 0x7ff62f10fe35"));
    disassembly_seq_detector.AddInstructionSequence(ad_same_target, SequenceMatchAntiDebugSameTarget, &disassembly_seq_detector);


    // Tecnica antidebug, verificada
    std::vector<std::vector<std::string>> ad_const_condition;
    ad_const_condition.push_back(CreateInstVector("xor eax, eax"));
    ad_const_condition.push_back(CreateInstVector("jz 0x7ff62f10fe35"));
    disassembly_seq_detector.AddInstructionSequence(ad_const_condition, SequenceMatchAntiDebugConstCondition, &disassembly_seq_detector);


    // para testes
    std::vector<std::vector<std::string>> read_string_cmp4;
    //read_string_cmp4.push_back(CreateInstVector("cmp byte ptr [rsi], 0x56"));
    //read_string_cmp4.push_back(CreateInstVector("cmp byte ptr [rbp], 0x43"));



    /*read_string_cmp4.push_back(CreateInstVector("sub rsp, 0x58"));
    read_string_cmp4.push_back(CreateInstVector("mov rdx, qword ptr [rcx]"));
    read_string_cmp4.push_back(CreateInstVector("jmp 0x7ff62f10fe09"));
    read_string_cmp4.push_back(CreateInstVector("mov eax, dword ptr [rdx]"));
    read_string_cmp4.push_back(CreateInstVector("jmp 0x7ff62f10fe0e"));
    read_string_cmp4.push_back(CreateInstVector("cmp eax, 0x4001000a"));
    read_string_cmp4.push_back(CreateInstVector("jmp 0x7ff62f10fe19"));
    read_string_cmp4.push_back(CreateInstVector("jz 0x7ff62f10fe35"));*/


    //seq_detector.AddInstructionSequence(read_string_cmp4, SequenceMatchAntiDebugSameTarget);
    //seq_detector.AddInstructionSequence(read_string_cmp4, SetZfToZero);




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

    INS_AddInstrumentFunction(TraceInstSeq, 0);

    //TraceInstructions::InitTrace(pid);

    TraceDisassembly::InitTraceDisassembly(pid, logsName);

    TraceMemory::InitMemoryTrace(pid, logsName);

    TraceFcnCall::InitFcnCallTrace(pid, logsName);


    if (KnobAllowAttachDebugger)
    {
        IMG_AddInstrumentFunction(InstrumentImageLoad, nullptr);
    }


    PIN_StartProgram();
    return 0;
}