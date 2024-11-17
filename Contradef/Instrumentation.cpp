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

std::string getFileName(const std::string& filePath) {
    // Encontra a última ocorrência de '\' ou '/'
    size_t pos = filePath.find_last_of("\\/");

    if (pos != std::string::npos) {
        return filePath.substr(pos + 1);
    }

    return filePath;
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

VOID EmitFuncCall(THREADID threadid, ADDRINT instAddress, ADDRINT targetRtn, BOOL isCall, BOOL isDirect, INT32 tailCall, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6)
{
    if (tailCall)
    {
        // A tail call is like an implicit return followed by an immediate call
    }

    if (!isDirect)
    {
        // Uma chamada indireta (!isDirect) é uma forma de invocar uma função quando o endereço da função é determinado em tempo de execução, em vez de ser codificado diretamente na instrução
    }

    PIN_LockClient();

    RTN rtn = RTN_FindByAddress(targetRtn);

    string s = StringFromAddrint(targetRtn);

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

                std::cout << " STRING -> " << WideStringFromPointer(addr) << std::endl;

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
        BOOL isCall = INS_IsCall(ins);
        // Indirect call
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
            IARG_THREAD_ID,
            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_BOOL, isCall, // isCall
            IARG_BOOL, FALSE, // isDirect
            IARG_BOOL, FALSE, // tailCall
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

         // Indirect call
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncCall),
            IARG_THREAD_ID,
            IARG_INST_PTR,
            IARG_BRANCH_TARGET_ADDR,
            IARG_BOOL, isCall, // isCall
            IARG_BOOL, FALSE, // isDirect
            IARG_BOOL, FALSE, // tailCall
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
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(EmitFuncReturn), IARG_THREAD_ID, IARG_INST_PTR,
            IARG_RETURN_IP, IARG_FUNCRET_EXITPOINT_VALUE, 
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





VOID aEmitRead(THREADID threadid, VOID* ea, UINT32 size)
{
    //ADDRINT address = reinterpret_cast<ADDRINT>(ea);

    //if (address >= region_start && address <= region_end)
    //{
    //    std::cout << "Read from address: " << std::hex << address << ", size: " << size << " bytes" << std::endl;
    //    // Podemos adicionar a lógica para copiar o valor da memória, por exemplo:
    //    UINT16 value;
    //    PIN_SafeCopy(&value, reinterpret_cast<UINT16*>(ea), size);
    //    std::cout << "Value read: " << value << std::endl;
    //}


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
    if (INS_IsMemoryRead(ins) && !INS_IsPrefetch(ins) && INS_IsStandardMemop(ins))
    {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(aEmitRead), IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE,
            IARG_END);
    }
}






static const ADDRINT target_address = 140699663292309;
static bool search = false;

// Função chamada antes da execução da instrução desejada
void CaptureLeaData(ADDRINT ins_addr, CONTEXT* ctxt, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6, ADDRINT arg7, ADDRINT arg8)
{
    if (true)
    {
        //RETORNE STRINGS DE UMA LISTA, PROVALVELMENTE AS QUE ESTAVAM ENCRIPTADAS

        // Obtém os valores dos registradores RAX e R11 do contexto
        ADDRINT rax = PIN_GetContextReg(ctxt, REG_RAX);
        ADDRINT r11 = PIN_GetContextReg(ctxt, REG_R11);

        // Calcula o valor do ponteiro usado na instrução LEA
        ADDRINT result = rax + (r11 * 2) + 0x4;

        // Imprime os valores capturados
        std::cout << "Instruction at address: " << std::hex << ins_addr << std::endl;
        std::cout << "RAX: " << std::hex << rax << ", R11: " << std::hex << r11 << std::endl;
        std::cout << "Calculated LEA result: " << std::hex << result << std::endl;

        wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);
        wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);

        std::wstring wstr(CharStr);
        wprintf(L" ** RETURN WString --> %ls <-\n", wstr.c_str());

        if (wstr.find(L"CDebugger detected - please disable it and restart the application.") != std::wstring::npos) {
            std::cout << "Debug achado: " << std::endl;

            search = true;
        }

        ADDRINT arg = arg7;

        //wchar_t* CharStr1 = reinterpret_cast<wchar_t*>(arg);
        //wprintf(L" ** RETURN WStringArg --> %ls\n", CharStr1);


        //char* CharStr2 = reinterpret_cast<char*>(arg);
        //std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;

        /*int* CharStr3 = reinterpret_cast<int*>(arg);
        std::cout << " ** RETURN INTArg --> " << *CharStr3 << std::endl;*/


        //char* CharStr = reinterpret_cast<char*>(result);
        //std::cout << " result - " << CharStr << std::endl;
    }
}


// Função que redireciona a execução para o endereço de destino caso o desvio (jnz) não seja tomado
void SimulaJnzComoJz(ADDRINT enderecoDestino, BOOL desvioTomado, CONTEXT* ctxt) {

    if (search == false) {
        return;
    }
    search == false;

    //std::cout << "INST " << std::endl;

    // Verifica se o desvio não foi tomado (como seria em um jz)
    if (!desvioTomado) {
        // Redireciona a execução para o endereço destino
        PIN_SetContextReg(ctxt, REG_INST_PTR, enderecoDestino);
        PIN_ExecuteAt(ctxt);
        std::cout << "ALTERANDO " << std::endl;

    }

}

//Verificar jp 0x7ff73153dc34

void aInstruction(INS ins, VOID* v)
{

    // Verifica se a instrução é do tipo LEA e se está no endereço desejado
    //std::cout << "Addr " << INS_Address(ins) << std::endl;
    if (INS_Disassemble(ins) == "lea rax, ptr [rax+r11*2+0x4]" && INS_Opcode(ins) == XED_ICLASS_LEA)
    {
        // Adiciona um callback antes da instrução ser executada
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CaptureLeaData,
            IARG_INST_PTR,     // Endereço da instrução
            IARG_CONTEXT,      // Contexto atual
            IARG_FUNCARG_CALLSITE_VALUE, 0,
            IARG_FUNCARG_CALLSITE_VALUE, 1,
            IARG_FUNCARG_CALLSITE_VALUE, 2,
            IARG_FUNCARG_CALLSITE_VALUE, 3,
            IARG_FUNCARG_CALLSITE_VALUE, 4,
            IARG_FUNCARG_CALLSITE_VALUE, 5,
            IARG_FUNCARG_CALLSITE_VALUE, 6,
            IARG_FUNCARG_CALLSITE_VALUE, 7,
            IARG_FUNCARG_CALLSITE_VALUE, 8,
            IARG_END);
    }
    else if (INS_Opcode(ins) == XED_ICLASS_JNZ && search == true) {
        // Obtém o endereço de destino da instrução jnz
        ADDRINT enderecoDestino = INS_DirectBranchOrCallTargetAddress(ins);

        // Insere uma chamada antes da execução de cada jnz
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)SimulaJnzComoJz,
            IARG_ADDRINT, enderecoDestino,
            IARG_BRANCH_TAKEN,
            IARG_CONTEXT,
            IARG_END);
        search == false;

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


VOID TraceFunc(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            //InstrumentFuncCall(trace, ins);
            seq_detector.InstructionTrace(ins, &seq_detector);
        }
    }
}

int InitInstrunctionsInstrumentation()
{

    std::vector<std::string> sequence;
    //// Tecnicas evasivas obsiium 1.4.3 Antidbi
    //sequence.push_back("jmp 0x7ff6acccdc2a");
    //sequence.push_back("test word ptr [rdi+0xa], 0x2");
    //sequence.push_back("jmp 0x7ff6226ddc33");
    //sequence.push_back("jz 0x7ff6226dd7c2");
    ////sequence.push_back("jmp 0x7ff6226dd9b2");
    ////sequence.push_back("test rax, rax");
    ////sequence.push_back("jmp 0x7ff6226dd9b8");
    ////sequence.push_back("jz 0x7ff6226dd7c2");
    //seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback);

    //// Tecnicas evasivas obsiium 1.4.3 AntiVM
    //std::vector<std::string> sequence2;
    //sequence2.push_back("jmp 0x7ff6acccdc2a");
    //sequence2.push_back("test word ptr [rsi+0xa], 0x2");
    //sequence2.push_back("jmp 0x7ff6226ddc33");
    //sequence2.push_back("jz 0x7ff6226dd7c2");
    //seq_detector.AddInstructionSequence(sequence2, Sequence2MatchCallback);




    // Tecnicas evasivas obsiium 1.8.5 Antidbi
    sequence.push_back("jmp 0x7ff6f499d6e1");
    sequence.push_back("test word ptr [rsi+0xa], 0x2");
    sequence.push_back("jnb 0x7ff6f499d6eb");
    sequence.push_back("jz 0x7ff6f499d2da");
    //sequence.push_back("jmp 0x7ff6f499d928");
    //sequence.push_back("test rax, rax");
    //sequence.push_back("jo 0x7ff6f499d8cd");
    //sequence.push_back("jz 0x7ff6f499d2da");
    
    seq_detector.AddInstructionSequence(sequence, SequenceMatchCallback);


    // Tecnicas evasivas obsiium 1.8.5 AntVM
    std::vector<std::string> sequence2;
    sequence2.push_back("jmp 0x7ff6f499d6e1");
    sequence2.push_back("test word ptr [rdi+0xa], 0x2");
    sequence2.push_back("jnb 0x7ff6f499d6eb");
    sequence2.push_back("jz 0x7ff6f499d2da");

    seq_detector.AddInstructionSequence(sequence2, Sequence2MatchCallback);

    // Tecnicas evasivas obsiium 1.8.5 Antidbi
    std::vector<std::string> sequence3;
    sequence3.push_back("jmp 0x7ff6f499d6e1");
    sequence3.push_back("test word ptr [rdi+0xa], 0x2");
    sequence3.push_back("jno 0x7ff6f499d6eb");
    sequence3.push_back("jz 0x7ff6f499d2da");

    seq_detector.AddInstructionSequence(sequence3, Sequence2MatchCallback);

    // Tecnicas evasivas obsiium 1.8.5 Antidbi
    std::vector<std::string> sequence4;
    sequence4.push_back("jmp 0x7ff6f499d6e1");
    sequence4.push_back("test word ptr [rsi+0xa], 0x2");
    sequence4.push_back("jno 0x7ff6f499d6eb");
    sequence4.push_back("jz 0x7ff6f499d2da");

    seq_detector.AddInstructionSequence(sequence4, Sequence2MatchCallback);

    seq_detector.Initialize();

    
    TRACE_AddInstrumentFunction(TraceFunc, 0);

    return 0;
}