#include "Instrumentation.h"

//CallStackManager callStackManager;
std::map<THREADID, CallStackManager> callStacks;
DllFunctionTracker callTracker(1000); // Máximo de 10 funções rastreadas

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

std::string getFileName(const std::string& filePath) {
    // Encontra a última ocorrência de '\' ou '/'
    size_t pos = filePath.find_last_of("\\/");

    if (pos != std::string::npos) {
        return filePath.substr(pos + 1);
    }

    return filePath;
}



bool IsValidWideString(const std::wstring& str) {
    for (wchar_t ch : str) {
        // Verifica se o caractere é alfanumérico ou pertence ao conjunto permitido
        if (!std::iswalnum(ch) && ch != L'.' && ch != L'-' && ch != L'_' && ch != L':' && ch != L'\\' && ch != L'/') {
            return false; // Se um caractere inválido for encontrado, retorna false
        }
    }
    if (str.size() < 3) {
        return false;
    }
    return true; // Todos os caracteres são válidos
}

bool IsValidString(const std::string& str) {
    for (char ch : str) {
        // Verifica se o caractere é alfanumérico ou pertence ao conjunto permitido
        if (!std::isalnum(ch) && ch != '.' && ch != '-' && ch != '_' && ch != ':' && ch != '\\' && ch != '/') {
            return false; // Se um caractere inválido for encontrado, retorna false
        }
    }
    if (str.size() < 3) {
        return false;
    }
    return true; // Todos os caracteres são válidos
}

std::wstring aCopyLPCWSTR(ADDRINT addr) {
    const size_t MAX_STRING_LENGTH = 20; // Defina o tamanho máximo da string
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
    const size_t MAX_STRING_LENGTH = 20; // Defina o tamanho máximo da string
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


VOID GetSectionInfo(IMG img, std::ofstream& OutFile)
{
    OutFile << std::endl << "[+] Informação de seções..." << std::endl;
    OutFile << "    Nome da imagem: " << std::string(IMG_Name(img)) << std::endl;

    // Índice da seção
    int index = 0;

    // Itera sobre as seções da imagem
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec), ++index)
    {
        std::string secName = SEC_Name(sec);
        secName = secName.empty() ? "[VAZIO]" : secName;
        ADDRINT secAddr = SEC_Address(sec);
        UINT64 secSize = SEC_Size(sec);

        OutFile << "    Seção [" << index << "]" << std::endl;
        OutFile << "        Nome da seção: " << secName << std::endl;
        OutFile << "        Endereço: 0x" << std::hex << static_cast<uint64_t>(secAddr) << std::dec << std::endl;
        OutFile << "        Tamanho: " << static_cast<uint64_t>(secSize) << " bytes" << std::endl;

        OutFile << "        Características: " << std::endl;
        if (SEC_IsExecutable(sec))
        {
            OutFile << "            A seção é executável." << std::endl;
        }
        if (SEC_IsReadable(sec))
        {
            OutFile << "            A seção é legível." << std::endl;
        }
        if (SEC_IsWriteable(sec))
        {
            OutFile << "            A seção é gravável." << std::endl;
        }
    }

    OutFile << "[*] Concluído" << std::endl << std::endl;

    OutFile.flush();  // Esvazia o buffer de saída
}


///////////////////


EXCEPT_HANDLING_RESULT errCB (THREADID tid, EXCEPTION_INFO* pExceptInfo, PHYSICAL_CONTEXT* pPhysCtxt, VOID* v)
{
    return EXCEPT_HANDLING_RESULT::EHR_HANDLED;
}

string tgt = "CreateFileW";
bool isWstring = 1;

VOID EmitFuncCall(THREADID threadid, ADDRINT instAddress, ADDRINT targetRtnAddr, BOOL isCall, BOOL isDirect, INT32 tailCall, ADDRINT returnAddress, ADDRINT rspValue, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
{

    if (IsMainExecutable(instAddress)) {
        PIN_LockClient(); 
        RTN rtnTgt = RTN_FindByAddress(targetRtnAddr);
        std::string s;
        bool print = true;
        std::string rtnName = "";
        std::string imgName = "";

        if (RTN_Valid(rtnTgt))
        {
            IMG img = SEC_Img(RTN_Sec(rtnTgt));
            if (IMG_Valid(img))
            {
                imgName = IMG_Name(img);
                s += imgName;
                if (imgName == IMG_Name(IMG_FindImgById(1))) {
                    print = false;
                }
            }
            rtnName = RTN_Name(rtnTgt);
            
            s += ":" + rtnName;
            if (rtnName.empty() || imgName.empty()) {
                print = false;
            }
        }
        else {
            print = false;
        }

        UINT64 stackValue = *reinterpret_cast<UINT64*>(rspValue);
        callStacks[threadid].pushFunction(instAddress, returnAddress, stackValue, tailCall, rtnName);

        if (print) {
            //std::cout << "----> " << instAddress << " -> " << targetRtn << s << std::endl;
        }

        PIN_UnlockClient();
        
    }

    PIN_LockClient();
    if (IsMainExecutable(instAddress) && !IsMainExecutable(targetRtnAddr)) {
        string tgtRtnName = "";
        string imgName = "";
        RTN tgtRtn = RTN_FindByAddress(targetRtnAddr);
        if (RTN_Valid(tgtRtn))
        {
            tgtRtnName = RTN_Name(tgtRtn);
            IMG tgtImg = SEC_Img(RTN_Sec(tgtRtn));
            if (IMG_Valid(tgtImg))
            {
                imgName = IMG_Name(tgtImg);
            }
        }
        if (!tgtRtnName.empty()) {
            callTracker.addFunctionCall(threadid, imgName, tgtRtnName, RTN_Address(tgtRtn));
        }
    }
    PIN_UnlockClient();


    return;





    if (tailCall)
    {
        // A tail call is like an implicit return followed by an immediate call
    }

    if (!isDirect)
    {
        // Uma chamada indireta (!isDirect) é uma forma de invocar uma função quando o endereço da função é determinado em tempo de execução, em vez de ser codificado diretamente na instrução
    }

    PIN_LockClient();

    RTN rtn = RTN_FindByAddress(targetRtnAddr);

    string s = StringFromAddrint(targetRtnAddr);

    if (RTN_Valid(rtn))
    {
        if (RTN_Name(rtn) != tgt) {
            PIN_UnlockClient();
            return;
        }

        IMG img = SEC_Img(RTN_Sec(rtn));
        s += " ";
        if (IMG_Valid(img))
        {
            s += IMG_Name(img) + ":";
        }

        s += RTN_Name(rtn);

        std::cout << s << " [T" << threadid << "] (" << arg0 << ", " << arg1 << ", " << arg2 << ", " << arg3 << ", " << arg4 << ", ...)" << std::endl;

        ADDRINT addr = arg0;  // Substitua por um endereço válido em um contexto real.

        if (isWstring)
        {

            if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(addr)))
            {
                // Leitura de uma wstring a partir do endereço dado
                //wchar_t* wideCharStr = reinterpret_cast<wchar_t*>(addr);
                //wprintf(L" ** WString --> %ls\n", wideCharStr);

                std::cout << " STRING -> " << WideStringToStringFromPointer(addr) << std::endl;

            }
            else
            {
                std::cout << "Endereço " << StringFromAddrint(addr) << " não está apto para leitura." << std::endl;
            }
        }
        else
        {
            if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(addr)))
            {
                std::string aBuffer = ConvertAddrToAnsiString(addr);
                std::cout << " STRING -> " << aBuffer << std::endl;
            }
            else
            {
                std::cout << "Endereço " << StringFromAddrint(addr) << " não está apto para leitura." << std::endl;
            }
        }
    }

    PIN_UnlockClient();
}


VOID EmitFuncReturn(THREADID threadid, ADDRINT instAddress, ADDRINT retAddr, ADDRINT ret0, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
{
    if (IsMainExecutable(retAddr)) {
        callStacks[threadid].popUntilAddress(retAddr);
        //callStacks[threadid].popFunction();
        //callStackManager.peekTopFunction();
    }
    //std::cout << " returns: " << ret0 << ", ret addr: " << retAddr << std::endl;


    
    return;
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(instAddress);
    PIN_UnlockClient();

    if (RTN_Valid(rtn))
    {
        if (RTN_Name(rtn) != tgt) return;

        std::cout << RTN_Name(rtn) << " [T" << threadid << "] returns: " << ret0 << ", ret addr: " << StringFromAddrint(retAddr) << std::endl;

    }

}

VOID InstrumentFuncCall(TRACE trace, INS ins)
{
    //if (KnobTraceOnlyMain && !IsMainExecutable(INS_Address(ins))) return;

    if (INS_IsCall(ins) && !INS_IsDirectControlFlow(ins))
    {
        //std::cout << INS_Disassemble(ins) << std::endl;
        BOOL isCall = INS_IsCall(ins);
        // Indirect call
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
            IARG_THREAD_ID,
            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_BOOL, isCall, // isCall
            IARG_BOOL, FALSE, // isDirect
            IARG_BOOL, FALSE, // tailCall
            IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
            IARG_REG_VALUE, REG_STACK_PTR,
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2,
            IARG_FUNCARG_CALLSITE_VALUE, 3,
            IARG_FUNCARG_CALLSITE_VALUE, 4,
            IARG_FUNCARG_CALLSITE_VALUE, 5,
            IARG_FUNCARG_CALLSITE_VALUE, 6,
            IARG_END);
    }
    else if (!INS_IsRet(ins) && INS_IsIndirectControlFlow(ins))
    {
        BOOL isCall = INS_IsCall(ins);
        //std::cout << INS_Disassemble(ins) << std::endl;

         // Indirect call
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
            IARG_THREAD_ID,
            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_BOOL, isCall, // isCall
            IARG_BOOL, FALSE, // isDirect
            IARG_BOOL, FALSE, // tailCall
            IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
            IARG_REG_VALUE, REG_STACK_PTR,
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2,
            IARG_FUNCARG_CALLSITE_VALUE, 3,
            IARG_FUNCARG_CALLSITE_VALUE, 4,
            IARG_FUNCARG_CALLSITE_VALUE, 5,
            IARG_FUNCARG_CALLSITE_VALUE, 6,
            IARG_END);
    }
    else if (INS_IsDirectControlFlow(ins))
    {
        // Is this a tail call?
        RTN sourceRtn = TRACE_Rtn(trace);
        RTN destRtn = RTN_FindByAddress(INS_DirectControlFlowTargetAddress(ins));
        //std::cout << INS_Disassemble(ins) << std::endl;

        if (INS_IsCall(ins)         // conventional call
            || sourceRtn != destRtn // tail call
            )
        {
            BOOL isCall = INS_IsCall(ins);
            BOOL tailcall = !isCall;
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
                IARG_THREAD_ID,
                IARG_INST_PTR,
                IARG_ADDRINT, RTN_Address(destRtn),
                IARG_BOOL, isCall, // isCall
                IARG_BOOL, TRUE, // isDirect
                IARG_BOOL, tailcall,
                IARG_ADDRINT, INS_NextAddress(ins), // Endereço da próxima instrução
                IARG_REG_VALUE, REG_STACK_PTR,
                IARG_FUNCARG_CALLSITE_VALUE, 0,
                IARG_FUNCARG_CALLSITE_VALUE, 1,
                IARG_FUNCARG_CALLSITE_VALUE, 2,
                IARG_FUNCARG_CALLSITE_VALUE, 3,
                IARG_FUNCARG_CALLSITE_VALUE, 4,
                IARG_FUNCARG_CALLSITE_VALUE, 5,
                IARG_FUNCARG_CALLSITE_VALUE, 6,
                IARG_END);
        }
    }
    else if (INS_IsRet(ins))
    {
        RTN rtn = TRACE_Rtn(trace);

#if defined(TARGET_LINUX) && defined(TARGET_IA32)
        //        if( RTN_Name(rtn) ==  "_dl_debug_state") return;
        if (RTN_Valid(rtn) && RTN_Name(rtn) == "_dl_runtime_resolve") return;
#endif
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncReturn), 
            IARG_THREAD_ID, 
            IARG_INST_PTR,
            IARG_RETURN_IP, 
            IARG_FUNCRET_EXITPOINT_VALUE, 
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2,
            IARG_FUNCARG_CALLSITE_VALUE, 3,
            IARG_FUNCARG_CALLSITE_VALUE, 4,
            IARG_FUNCARG_CALLSITE_VALUE, 5,
            IARG_FUNCARG_CALLSITE_VALUE, 6,
            IARG_END);

    }
    else {
        if (INS_IsDirectControlFlow(ins)) {

            std::cout << INS_Disassemble(ins) << "não \n";
        }
    }
}







// Função que redireciona a execução para o endereço de destino caso o desvio (jnz) não seja tomado
void SimulaJnzComoJz(ADDRINT enderecoDestino, BOOL desvioTomado, CONTEXT* ctxt) {


    //std::cout << "INST " << std::endl;

    // Verifica se o desvio não foi tomado (como seria em um jz)
    if (!desvioTomado) {
        // Redireciona a execução para o endereço destino
        PIN_SetContextReg(ctxt, REG_INST_PTR, enderecoDestino);
        PIN_ExecuteAt(ctxt);
        std::cout << "ALTERANDO " << std::endl;

    }

}


InstructionSequenceDetector seq_detector;
//
//criar um tipo de objeto com nome e um cb para executar o cb sempre que dé match numa sequencía
//
//outra opção é usar uma lista de CBs para cada endereço e registrar as execuções junto com os matchs 

#include "xed-interface.h"
//
//VOID InstructionX(INS ins, VOID* v)
//{
//    // Obter o endereço da instrução
//    ADDRINT addr = INS_Address(ins);
//
//    // Obter o tamanho da instrução
//    UINT32 len = INS_Size(ins);
//
//    // Obter o código binário da instrução
//    UINT8* code = new UINT8[len];
//    PIN_SafeCopy(code, (VOID*)addr, len);
//
//    // Inicializar o estado do XED
//    xed_state_t dstate;
//    xed_state_zero(&dstate);
//    dstate.stack_addr_width = XED_ADDRESS_WIDTH_64b;
//    dstate.mmode = XED_MACHINE_MODE_LONG_64;
//
//    // Decodificar a instrução original
//    xed_decoded_inst_t xedd;
//    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
//
//    xed_error_enum_t xed_error = xed_decode(&xedd, code, len);
//    if (xed_error == XED_ERROR_NONE)
//    {
//        // Modificar a instrução conforme necessário
//        // Exemplo: alterar o opcode para NOP
//        xed_encoder_instruction_t enc_instr;
//        xed_inst1(&enc_instr, dstate, XED_ICLASS_NOP, 0);
//
//        // Codificar a nova instrução
//        UINT8 enc_buf[XED_MAX_INSTRUCTION_BYTES];
//        UINT32 enc_len = 0;
//
//        xed_encoder_request_t enc_req;
//        xed_encoder_request_zero_set_mode(&enc_req, &dstate);
//
//        xed_bool_t convert_ok = xed_convert_to_encoder_request(&enc_req, &enc_instr);
//        if (convert_ok)
//        {
//            xed_error_enum_t xed_err = xed_encode(&enc_req, enc_buf, XED_MAX_INSTRUCTION_BYTES, &enc_len);
//            if (xed_err == XED_ERROR_NONE)
//            {
//                // Substituir a instrução no cache de código do PIN
//                // Use INS_Rewrite para substituir a instrução (Note que INS_Rewrite não existe, mas veja a seção abaixo)
//
//                // Infelizmente, o PIN não fornece uma API direta para substituir instruções individuais
//                // Alternativamente, você pode remover a instrução original e inserir a nova instrução
//
//                // Remover a instrução original
//                INS_Delete(ins);
//
//                // Inserir a nova instrução codificada
//                INS_InsertDirectJump(ins, IPOINT_BEFORE, (ADDRINT)enc_buf);
//                // Note que INS_InsertDirectJump é usado para inserir um salto, não uma instrução arbitrária
//                // Inserir instruções arbitrárias requer técnicas avançadas
//
//                // Outra opção é usar um código de substituição personalizado
//            }
//        }
//    }
//
//    delete[] code;
//}


VOID SequenceMatchCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obtém as últimas 2 chamadas
    auto lastCalls = callTracker.getLastNCalls(100);
    std::cout << "Últimas 100 chamadas:\n";
    for (const auto& call : lastCalls) {
        std::cout << "[" << call.threadId << "] " << std::hex << call.address << " - " << std::dec << "DLL: " << call.dllName << ", Função: " << call.functionName << "\n";
    }


    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;
    //std::cout << "1 - ZF ->" << zf_before << std::endl;

    if (zf_before == 0) {

        // Para setar o ZF para 1:
        flags |= (1 << 6); // Seta o bit 6 (ZF) para 1
        std::cout << "1 - APLICANDO CONTRAMEDIDA NO INDICE -> " << sequence.matchCount << std::endl;

        // Para setar o ZF para 0:
        //flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

        // Atualizar os flags
        PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        // Obter os flags atualizados
        flags = PIN_GetContextReg(ctxt, REG_RFLAGS);
        BOOL zf_after = (flags & (1 << 6)) != 0;
        //std::cout << "ZF depois da modificação: " << zf_after << std::endl;

        PIN_ExecuteAt(ctxt);
    }









    //ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RAX);
    //ADDRINT r11_value = PIN_GetContextReg(ctxt, REG_R11);
    ////std::cout << "RAX = 0x" << std::hex << rax_value << std::endl;

    //ADDRINT result = rax_value + (r11_value * 2) + 0x4;

    //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);

    //std::wstring wstr(CharStr);

    //if (wstr.find(L"CDebugger detected - please disable it and restart the application.") != std::wstring::npos) {
    //    std::cout << "Sequência encontrada na thread " << tid << ", Instaddr: " << ip << ":\n";

    //    /*std::cout << "Debug achado: Fazendo patch" << std::endl;
    //    CharStr[5] = L'X';
    //    CharStr[7] = L'Q';
    //    CharStr[9] = L'Y';*/


    //    wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);



    //    // OBTENDO A PROXIMA STRING DINAMICAMENTE
    //    uint8_t* rax = reinterpret_cast<uint8_t*>(rax_value);

    //    uint16_t value16 = *reinterpret_cast<uint16_t*>(rax + 512);
    //    uint32_t r11d = static_cast<uint32_t>(value16);

    //    // Cálculo do novo valor de rax (lea rax, ptr [rax+r11*2+0x4])
    //    rax = rax + r11d * 2 + 0x4;

    //    // Impressão do novo valor do ponteiro
    //    std::cout << "Novo valor de rax: " << static_cast<void*>(rax) << std::endl;


    //    wchar_t* CharStr2 = reinterpret_cast<wchar_t*>(rax);
    //    fflush(stdout);
    //    wprintf(L" ** RETURN WString2 --> %ls <-\n", CharStr2);
    //    fflush(stdout);
    //}








    // Incrementar RAX
    //PIN_SetContextReg(ctxt, REG_RAX, rax_value + 1);

    // Ler RAX após modificação
    //ADDRINT new_rax_value = PIN_GetContextReg(ctxt, REG_RAX);
    //std::cout << "Depois: RAX = 0x" << std::hex << new_rax_value << std::endl;

    //for (const auto& instr : sequence)
    //{
    //    std::cout << instr << "\n";
    //}





    // ADDRINT r13_value = PIN_GetContextReg(ctxt, REG_R13);
    ////std::cout << "RAX = 0x" << std::hex << rax_value << std::endl;

    // ADDRINT result = r13_value + 0x4;

    //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);
    //fflush(stdout);
    //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
    //fflush(stdout);
    //std::wstring wstr(CharStr);

    //if (wstr.find(L"Error resolving imported function") != std::wstring::npos) {
    //    std::cout << "Debug achado: Fazendo patch" << std::endl;
    //    CharStr[5] = L'X';
    //    CharStr[7] = L'Q';
    //    CharStr[9] = L'Y';
    //}
}

VOID SequenceMatchCallbackx(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;
    //std::cout << "1 - ZF ->" << zf_before << std::endl;

    if (zf_before == 0) {

        // Para setar o ZF para 1:
        flags |= (1 << 6); // Seta o bit 6 (ZF) para 1
        std::cout << "2 - APLICANDO CONTRAMEDIDA NO INDICE -> " << sequence.matchCount << std::endl;

        // Para setar o ZF para 0:
        //flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

        // Atualizar os flags
        PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        // Obter os flags atualizados
        flags = PIN_GetContextReg(ctxt, REG_RFLAGS);
        BOOL zf_after = (flags & (1 << 6)) != 0;
        //std::cout << "ZF depois da modificação: " << zf_after << std::endl;

        PIN_ExecuteAt(ctxt);
    }


}

VOID SequenceMatchCallback2(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;
    std::cout << "2 - VERIFICANDO O INDICE -> " << sequence.matchCount << " - ZF ->" << zf_before << std::endl;

    if (zf_before == 0) {

        //// Para setar o ZF para 1:
        //flags |= (1 << 6); // Seta o bit 6 (ZF) para 1

        //// Para setar o ZF para 0:
        //flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

        //// Atualizar os flags
        //PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        //// Obter os flags atualizados
        //flags = PIN_GetContextReg(ctxt, REG_RFLAGS);
        //BOOL zf_after = (flags & (1 << 6)) != 0;
        ////std::cout << "ZF depois da modificação: " << zf_after << std::endl;

        //PIN_ExecuteAt(ctxt);
    }


}

VOID SequenceMatchCallback3(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;

    if (zf_before == 1) {
        std::cout << "2 - VERIFICANDO O INDICE -> " << sequence.matchCount << std::endl;

        // Para setar o ZF para 1:
        //flags |= (1 << 6); // Seta o bit 6 (ZF) para 1

        // Para setar o ZF para 0:
        flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

        // Atualizar os flags
        PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        // Obter os flags atualizados
        flags = PIN_GetContextReg(ctxt, REG_RFLAGS);
        BOOL zf_after = (flags & (1 << 6)) != 0;
        //std::cout << "ZF depois da modificação: " << zf_after << std::endl;

        PIN_ExecuteAt(ctxt);
    }


}


VOID Sequence2MatchCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;
    //std::cout << "ZF antes da modificação: " << zf_before << std::endl;

    // Para setar o ZF para 1:
    flags |= (1 << 6); // Seta o bit 6 (ZF) para 1

    // Para setar o ZF para 0:
    //flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0

    // Atualizar os flags
    PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

    // Obter os flags atualizados
    flags = PIN_GetContextReg(ctxt, REG_RFLAGS);
    BOOL zf_after = (flags & (1 << 6)) != 0;
    //std::cout << "ZF depois da modificação: " << zf_after << std::endl;

    PIN_ExecuteAt(ctxt);

}

VOID SequenceMatchCallFcnCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT targetAddr = PIN_GetContextReg(ctxt, REG_R10);



    //////

    if (IsMainExecutable(ip)) {
        PIN_LockClient();
        RTN rtnTgt = RTN_FindByAddress(targetAddr);
        std::string s;
        bool print = true;
        if (RTN_Valid(rtnTgt))
        {
            IMG img = SEC_Img(RTN_Sec(rtnTgt));
            std::string imgName = "";
            if (IMG_Valid(img))
            {
                imgName = IMG_Name(img);
                s += imgName;
                if (imgName == IMG_Name(IMG_FindImgById(1))) {
                    print = false;
                }
            }
            std::string rtnName = RTN_Name(rtnTgt);
            s += ":" + rtnName;
            if (rtnName.empty() || imgName.empty()) {
                print = false;
            }
        }
        else {
            print = false;
        }
        if (print) {
            std::cout << "----> " << ip << " -> " << targetAddr << s << std::endl;
        }

        PIN_UnlockClient();

    }
    return;

    //////



    //PIN_LockClient();

    //ADDRINT targetAddr = PIN_GetContextReg(ctxt, REG_R10);

    //IMG img = IMG_FindByAddress(targetAddr);


    //if (IMG_Valid(img)) {
    //    std::cout << "Chamada -> " << IMG_Name(img) << ":";

    //}
    //RTN rtn = RTN_FindByAddress(targetAddr);
    //if (RTN_Valid(rtn)) {
    //    std::cout << RTN_Name(rtn);
    //}
    //std::cout << std::endl;

    //PIN_UnlockClient();
}


VOID SequenceMatchReadStringFcnCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT regRcx = PIN_GetContextReg(ctxt, REG_RCX);

    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(regRcx)))
    {
        std::cout << "R9 = 0x" << std::hex << regRcx << std::endl;
        char* CharStr = reinterpret_cast<char*>(regRcx);
        std::cout << "Nome da função --> " << CharStr << std::endl;
    }
}


VOID SequenceMatchReadStringEnvsCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RAX);
    ADDRINT rbp_value = PIN_GetContextReg(ctxt, REG_RBP);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rbp_value + rax_value * 2;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));


        char* CharStr2 = reinterpret_cast<char*>(result);
        std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
    }


}



VOID SequenceMatchReadStringEnvs2Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RAX);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_R8);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = r8_value + rax_value * 1;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));


        char* CharStr2 = reinterpret_cast<char*>(result);
        std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
    }


}


VOID SequenceMatchReadStringEnvs3Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RSI);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_RBP);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rax_value + r8_value * 1;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));


        char* CharStr2 = reinterpret_cast<char*>(result);
        std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
    }


}


VOID SequenceMatchReadStringStrsCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_R10);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_R9);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rax_value;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        //wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));


        char* CharStr2 = reinterpret_cast<char*>(result);
        if (IsValidString(std::string(CharStr2))) {
            std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
        }
    }


}




VOID SequenceMatchReadStringDbgNamesCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RCX);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_R9);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rax_value;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        std::wstring wstr = ConvertAddrToWideString(result);
        if (IsValidWideString(wstr)) {
            wprintf(L" ** RETURN WStringX --> %ls <-\n", wstr);

        }
        //wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));



        char* CharStr2 = reinterpret_cast<char*>(result);
        if (IsValidString(std::string(CharStr2))) {
            std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
        }

        //char* CharStr2 = reinterpret_cast<char*>(result);
        //std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;

    }


}



VOID SequenceMatchReadStringCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RDX);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_R9);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rax_value;


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    {
        //wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


        //std::wstring wstr(CharStr);
        //wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
        std::wstring wstr = ConvertAddrToWideString(result);
        if (IsValidWideString(wstr)) {
            wprintf(L" ** RETURN WStringX --> %ls <-\n", wstr);

        }
        //wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));



        char* CharStr2 = reinterpret_cast<char*>(result);
        if (IsValidString(std::string(CharStr2))) {
            std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
        }

        //char* CharStr2 = reinterpret_cast<char*>(result);
        //std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;

    }


}


VOID SequenceMatchReadStringForCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    std::cout << " ** --> " << std::endl;

    //lea rax, ptr [rip+0x25e]
    ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RAX);
    ADDRINT r8_value = PIN_GetContextReg(ctxt, REG_R9);

    //ADDRINT rdx_value = PIN_GetContextReg(ctxt, REG_RCX);

    ADDRINT result = rax_value;

    for (size_t i = 0; i < 15; i++)
    {

        if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
        {
            wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);


            std::wstring wstr(CharStr);
            wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
            /*std::wstring wstr = ConvertAddrToWideString(result);
            if (IsValidWideString(wstr)) {
                wprintf(L" ** RETURN WStringX --> %ls <-\n", wstr);

            }*/
            //wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(result));



            //char* CharStr2 = reinterpret_cast<char*>(result);
            //if (IsValidString(std::string(CharStr2))) {
            //    std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
            //}

            //char* CharStr2 = reinterpret_cast<char*>(result);
            //std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;

        }
        result = result - 0x4a;
    }


}



VOID aEmitRead(THREADID threadid, ADDRINT instAddress, std::string* insstr, VOID* ea, UINT32 size)
{
    ADDRINT address = reinterpret_cast<ADDRINT>(ea);


    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(ea)))
    {
        std::wstring wstr = aCopyLPCWSTR(address);
        if (IsValidWideString(wstr)) {
            std::cout << "Read from address: " << std::hex << address << ", size: " << size << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << *insstr << std::endl;
            wprintf(L" ** RETURN WString --> %ls <-\n", wstr);
        }
        
        std::string str = aCopyLPCSTR(address);
        if (IsValidString(str)) {
            std::cout << "Read from address: " << std::hex << address << ", size: " << size << " bytes" << " -> " << std::hex << instAddress << std::dec << " > " << *insstr << std::endl;
            std::cout << " ** RETURN String --> " << str << std::endl;
        }
                
    }
    //UINT16 value;
    //PIN_SafeCopy(&value, reinterpret_cast<UINT16*>(ea), size);
    //std::cout << "Value read: " << value << std::endl;

    return;

    switch (size)
    {
    case 0:
        std::cout << "0 repeat count" << std::endl;
        break;

    case 1:
    {
        UINT8 x;
        PIN_SafeCopy(&x, static_cast<UINT8*>(ea), 1);
        std::cout << static_cast<UINT32>(x) << " = *(UINT8*)" << ea << std::endl;
    }
    break;

    case 2:
    {
        UINT16 x;
        PIN_SafeCopy(&x, static_cast<UINT16*>(ea), 2);
        std::cout << x << " = *(UINT16*)" << ea << std::endl;
    }
    break;

    case 4:
    {
        UINT32 x;
        PIN_SafeCopy(&x, static_cast<UINT32*>(ea), 4);
        std::cout << x << " = *(UINT32*)" << ea << std::endl;
    }
    break;

    case 8:
    {
        UINT64 x;
        PIN_SafeCopy(&x, static_cast<UINT64*>(ea), 8);
        std::cout << x << " = *(UINT64*)" << ea << std::endl;
    }
    break;

    default:
        //ShowN(size, ea);
        std::cout << " = *(UINT" << std::dec << size * 8 << std::hex << ")" << ea << std::endl;
        break;
    }

}



VOID aMemoryTrace(INS ins)
{

    //if (INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
    //{
    //    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CaptureWriteEa), IARG_THREAD_ID, IARG_MEMORYWRITE_EA, IARG_END);

    //    if (INS_IsValidForIpointAfter(ins))
    //    {
    //        INS_InsertPredicatedCall(ins, IPOINT_AFTER, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE, IARG_END);
    //    }
    //    if (INS_IsValidForIpointTakenBranch(ins))
    //    {
    //        INS_InsertPredicatedCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(EmitWrite), IARG_THREAD_ID, IARG_MEMORYWRITE_SIZE,
    //            IARG_END);
    //    }
    //}
    string memString = INS_Disassemble(ins);
    if (INS_HasMemoryRead2(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aEmitRead), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new std::string(memString), IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE,
            IARG_END);
    }

    if (INS_IsMemoryRead(ins) && !INS_IsPrefetch(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aEmitRead), IARG_THREAD_ID, IARG_INST_PTR, IARG_PTR, new std::string(memString), IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
            IARG_END);
    }
}

VOID TraceFunc(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            InstrumentFuncCall(trace, ins);

            //aMemoryTrace(ins);
        }
    }
}


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



/////
#include <set>
constexpr BYTE SPECIAL_VALUE = 0xFFF34433;    // Valor a interceptar
constexpr BYTE MODIFIED_VALUE = 0xFFFFFFFF;  // Novo valor a ser escrito

std::set<ADDRINT> interceptedAddresses; // Endereços modificados

VOID ModifyValue(ADDRINT addr) {
    BYTE* target = reinterpret_cast<BYTE*>(addr);
    if (*target == SPECIAL_VALUE) {
        *target = MODIFIED_VALUE; // Altera o valor especial
        interceptedAddresses.insert(addr);
        std::cout << "Valor especial interceptado e alterado no endereço: " << std::hex << addr << std::endl;
    }
}

/////

VOID TraceInst(INS ins, VOID* v)
{
    seq_detector.InstructionTrace(ins, &seq_detector);

    // Intercepta instruções de escrita em memória
    //if (INS_IsMemoryWrite(ins)) {
    //    INS_InsertCall(
    //        ins, IPOINT_BEFORE, (AFUNPTR)ModifyValue,
    //        IARG_MEMORYWRITE_EA, // Endereço efetivo de escrita
    //        IARG_END);
    //}

    return;


    std::string disassembledInstr = INS_Disassemble(ins);
    // Identifica instruções do tipo LEA
    if (INS_Mnemonic(ins) == "LEA") {
        // Obtém o registrador de destino (primeiro operando)
        REG destReg = INS_OperandReg(ins, 0);

        // Instrumenta a instrução LEA para capturar o valor do registrador
        INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)RecordLEA,
            IARG_INST_PTR,            // Endereço da instrução
            IARG_PTR, new std::string(disassembledInstr),
            IARG_UINT32, destReg,     // Registrador de destino
            IARG_REG_VALUE, destReg,  // Valor do registrador após execução
            IARG_END);
    }
}

namespace standard = __std_alias;

int InitInstrunctionsInstrumentation()
{

    //// Tecnicas evasivas obsiium 1.4.3 Antidbi
    //sequence.push_back("jmp 0x7ff6acccdc2a");
    //sequence.push_back("test word ptr [rdi+0xa], 0x2");
    //sequence.push_back("jmp 0x7ff6226ddc33");
    //sequence.push_back("jz 0x7ff6226dd7c2");
    //seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback);

    //// Tecnicas evasivas obsiium 1.4.3 AntiVM
    //std::vector<std::string> sequence2;
    //sequence2.push_back("jmp 0x7ff6acccdc2a");
    //sequence2.push_back("test word ptr [rsi+0xa], 0x2");
    //sequence2.push_back("jmp 0x7ff6226ddc33");
    //sequence2.push_back("jz 0x7ff6226dd7c2");
    //seq_detector.AddInstructionSequence(sequence2, Sequence2MatchCallback);





    //Funciona parcialmente 1.4 e 1.8
    //std::vector<std::vector<std::string>> sequence2;

    //sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d6e1"));
    //sequence2.push_back(CreateInstVector("test word ptr [r?i+0xa], 0x2"));
    //sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d6eb", "jnb 0x7ff6f499d6eb", "jno 0x7ff6f499d6eb"));
    //sequence2.push_back(CreateInstVector("jz 0x7ff6f499d2da"));
    //sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d928"));
    //sequence2.push_back(CreateInstVector("test rax, rax"));
    //sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d8cd", "jo 0x7ff6f499d8cd"));
    //sequence2.push_back(CreateInstVector("jz 0x7ff6f499d2da"));

    //seq_detector.AddInstructionSequence(sequence2, SequenceMatchCallback2);






    ////Sequencia invalida, pesquisar mais sobre
    //std::vector<std::vector<std::string>> sequence3;

    //sequence3.push_back(CreateInstVector("test word ptr [rsi+0xa], 0x2"));
    //sequence3.push_back(CreateInstVector("jno 0x7ff6889700d1"));
    //sequence3.push_back(CreateInstVector("jz 0x7ff68896f179"));
    //sequence3.push_back(CreateInstVector("jmp 0x7ff68896f9d4"));
    //sequence3.push_back(CreateInstVector("test rax, rax"));
    //sequence3.push_back(CreateInstVector("jnb 0x7ff68896f9dc"));
    //sequence3.push_back(CreateInstVector("jnz 0x7ff68896f594"));

    //seq_detector.AddInstructionSequence(sequence3, SequenceMatchCallback3);






    //// Tecnicas evasivas obsiium 1.8.5 AntVM
    //std::vector<std::string> sequence2;
    //sequence2.push_back("jmp 0x7ff6f499d6e1");
    //sequence2.push_back("test word ptr [rdi+0xa], 0x2");
    //sequence2.push_back("jnb 0x7ff6f499d6eb");
    //sequence2.push_back("jz 0x7ff6f499d2da");

    //seq_detector.AddInstructionSequence(sequence2, Sequence2MatchCallback);

    //// Tecnicas evasivas obsiium 1.8.5 Antidbi
    //std::vector<std::string> sequence3;
    //sequence3.push_back("jmp 0x7ff6f499d6e1");
    //sequence3.push_back("test word ptr [rdi+0xa], 0x2");
    //sequence3.push_back("jno 0x7ff6f499d6eb");
    //sequence3.push_back("jz 0x7ff6f499d2da");

    //seq_detector.AddInstructionSequence(sequence3, Sequence2MatchCallback);

    //// Tecnicas evasivas obsiium 1.8.5 Antidbi
    //std::vector<std::string> sequence4;
    //sequence4.push_back("jmp 0x7ff6f499d6e1");
    //sequence4.push_back("test word ptr [rsi+0xa], 0x2");
    //sequence4.push_back("jno 0x7ff6f499d6eb");
    //sequence4.push_back("jz 0x7ff6f499d2da");

    //seq_detector.AddInstructionSequence(sequence4, Sequence2MatchCallback);






        // Tecnicas evasivas obsiium 1.8.5 Antidbi
    std::vector<std::vector<std::string>> sequence;
    sequence.push_back(CreateInstVector("jmp 0x7ff6f499d6e1"));
    sequence.push_back(CreateInstVector("test word ptr [r?i+0xa], 0x2"));
    sequence.push_back(CreateInstVector("jmp 0x7ff6f499d6eb", "jnb 0x7ff6f499d6eb", "jno 0x7ff6f499d6eb"));
    sequence.push_back(CreateInstVector("jz 0x7ff6f499d2da"));
    seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback, 33, 33); 
    //seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback); // TODAS




    ///TESTE
    // Testar em outra release do obsidium o motivo de ter funcionado apenas com uma alteração (mais acima)
    std::vector<std::vector<std::string>> sequencex;
    sequencex.push_back(CreateInstVector("jmp 0x7ff6f499d6e1"));
    sequencex.push_back(CreateInstVector("test word ptr [r?i+0xa], 0x2"));
    sequencex.push_back(CreateInstVector("jmp 0x7ff6f499d6eb", "jnb 0x7ff6f499d6eb", "jno 0x7ff6f499d6eb"));
    sequencex.push_back(CreateInstVector("jz 0x7ff6f499d2da"));
    //seq_detector.AddInstructionSequence(sequencex, SequenceMatchCallbackx, 31, 31); // não é mais necessário nessa release do obsidium, verificar o motivo
    ///FIM Teste














    // parte 2 da anterior
    std::vector<std::vector<std::string>> sequence2;
    sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d928"));
    sequence2.push_back(CreateInstVector("test rax, rax"));
    sequence2.push_back(CreateInstVector("jmp 0x7ff6f499d8cd", "jo 0x7ff6f499d8cd"));
    sequence2.push_back(CreateInstVector("jz 0x7ff6f499d2da"));
    //seq_detector.AddInstructionSequence(sequence2, SequenceMatchCallback2);


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



    // Variaveis de amboente
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("lea rax, ptr [rcx+0x1]"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmovz rax, rcx"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov rcx, rax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("or rax, 0xffffffffffffffff"));
    //sequence_read_string_fcn.push_back(CreateInstVector("inc rax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp byte ptr [r8+rax*1], 0x0"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringEnvs2Callback);




    ////Variaveis de amboente 3, contem PIN_CRT_TZDATA e VBOX_MSI_INSTALL_PATH
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("mov al, byte ptr [rsi]"));
    //sequence_read_string_fcn.push_back(CreateInstVector("test al, al"));
    //sequence_read_string_fcn.push_back(CreateInstVector("jz 0x7ff6d6ea14fa"));
    //sequence_read_string_fcn.push_back(CreateInstVector("or rbp, 0xffffffffffffffff"));
    //sequence_read_string_fcn.push_back(CreateInstVector("inc rbp"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp byte ptr [rsi+rbp*1], 0x0"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringEnvs3Callback);


    ////Algumas strings, nada importante
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("mov r10, rdi"));
    //sequence_read_string_fcn.push_back(CreateInstVector("sub r10, rax"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov al, byte ptr [r10]"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringStrsCallback);




    //Algumas strings, IMPORTNTE, segue a função GetModuleFileNameW, e precede a função openFile e closeHandle
    std::vector<std::vector<std::string>> sequence_read_string_fcn;
    sequence_read_string_fcn.push_back(CreateInstVector("jmp 0x7ff6d6ecf9d6"));
    sequence_read_string_fcn.push_back(CreateInstVector("jmp 0x7ff6d6ecf37b"));
    sequence_read_string_fcn.push_back(CreateInstVector("movzx eax, byte ptr [rcx]"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringStrings1Callback);



    ////Desconsiderar
    //std::vector<std::vector<std::string>> sequence_read_string_fcn;
    //sequence_read_string_fcn.push_back(CreateInstVector("xor r11d, r11d"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov r10, rdx"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov r8, rcx"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp word ptr [rdx], r11w"));
    //sequence_read_string_fcn.push_back(CreateInstVector("jnz 0x7ffdf81032b3"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp word ptr [rcx], r11w"));
    //sequence_read_string_fcn.push_back(CreateInstVector("jz 0x7ffdf81032f3"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov r9, rcx"));
    //sequence_read_string_fcn.push_back(CreateInstVector("sub r9, r10"));
    //sequence_read_string_fcn.push_back(CreateInstVector("mov rdx, r10"));
    //sequence_read_string_fcn.push_back(CreateInstVector("cmp word ptr [rdx], r11w"));
    //seq_detector.AddInstructionSequence(sequence_read_string_fcn, SequenceMatchReadStringCallback);

    seq_detector.Initialize();

    
    TRACE_AddInstrumentFunction(TraceFunc, 0);

    INS_AddInstrumentFunction(TraceInst, 0);


    return 0;
}