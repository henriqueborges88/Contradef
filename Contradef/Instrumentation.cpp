#include "Instrumentation.h"

KNOB< BOOL > KnobDisassembly(KNOB_MODE_WRITEONCE, "pintool", "dasm", "1", "Trace memory");

std::ofstream disassemblyTraceOut;

//CallStackManager callStackManager;
std::map<THREADID, CallStackManager> callStacks;
DllFunctionTracker callTracker(100000); // Máximo de 100000 funções rastreadas

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

VOID PauseAtAddress(ADDRINT address) {
    PIN_LockClient();
    std::cout << "[CONTRADEF] O Contradef pausou no endereço " << std::hex << address << std::endl;
    std::cout << "[CONTRADEF] Anexe o depurador ao processo agora." << std::endl;
    std::cout << "[CONTRADEF] Pressione Enter para continuar a execucao..." << std::endl;

    std::cin.get(); // Aguarda entrada do usuário para continuar
    PIN_UnlockClient();
    PIN_Detach();
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
        ADDRINT tgtRtnAddr = 0;
        RTN tgtRtn = RTN_FindByAddress(targetRtnAddr);
        if (RTN_Valid(tgtRtn))
        {
            tgtRtnName = RTN_Name(tgtRtn);
            tgtRtnAddr = RTN_Address(tgtRtn);
            IMG tgtImg = SEC_Img(RTN_Sec(tgtRtn));
            if (IMG_Valid(tgtImg))
            {
                imgName = IMG_Name(tgtImg);
            }
        }
        if (tgtRtnName.empty()) {
            OS_PROCESS_ID pid = PIN_GetPid();
            FunctionData fcnData = GetFunctionFromAddress(targetRtnAddr, pid);
            tgtRtnName = fcnData.functionName;
            tgtRtnAddr = targetRtnAddr;
            imgName = "[forced] " + fcnData.fullModuleName;
        }
        callTracker.addFunctionCall(threadid, imgName, tgtRtnName, tgtRtnAddr);
        string retRtnName = "INVALID_RTN";
        RTN rtnt = RTN_FindByAddress(returnAddress);
        if (RTN_Valid(rtnt)) {
            retRtnName = RTN_Name(rtnt);
        }
        //std::cout << std::hex << RTN_Address(tgtRtn) << " - " << imgName << ":" << tgtRtnName << " T[" << threadid << "] - Return to: " << retRtnName << " (" << returnAddress << ")" << std::dec << std::endl;

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
        PIN_LockClient();
        RTN sourceRtn = TRACE_Rtn(trace);
        RTN destRtn = RTN_FindByAddress(INS_DirectControlFlowTargetAddress(ins));
        PIN_UnlockClient();

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


InstructionSequenceDetector seq_detector;
InstructionSequenceDetector disassembly_seq_detector;


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
    //auto lastCalls = callTracker.getLastNCalls(100);
    //std::cout << "Últimas 100 chamadas:\n";
    //for (const auto& call : lastCalls) {
    //    std::cout << "[" << call.threadId << "] " << std::hex << call.address << " - " << std::dec << "DLL: " << call.dllName << ", Função: " << call.functionName << "\n";
    //}


    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;
    //std::cout << "1 - ZF ->" << zf_before << std::endl;

    if (zf_before == 0) {

        // Para setar o ZF para 1:
        flags |= (1 << 6); // Seta o bit 6 (ZF) para 1
        std::cout << "[CONTRADEF] APLICANDO CONTRAMEDIDA NO INDICE " << sequence.matchCount << std::endl;

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


    //    wchar_t* CharStr2 = reinterpret_cast<wchar_t*>(rax);
    //    fflush(stdout);
    //    wprintf(L" ** RETURN WString2 --> %ls <-\n", CharStr2);
    //    fflush(stdout);
    //}

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
    std::cout << "xxxxx: " << ip << std::endl;
    //return;
    ADDRINT regRcx = PIN_GetContextReg(ctxt, REG_RAX);

    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(regRcx)))
    {
        wprintf(L" ** RETURN WStringX --> %ls <-\n", ConvertAddrToWideString(regRcx));

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


VOID SequenceMatchReadStringCmpCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter o valor completo de RSI
    ADDRINT rsi_value = PIN_GetContextReg(ctxt, REG_RSI);

    // Extrair os 16 bits menos significativos (SI)
    UINT16 six_value = static_cast<UINT16>(rsi_value);

    // Imprimir o valor do registrador SI
    std::cout << "Valor de SI: 0x" << std::hex << six_value << std::endl;
    std::cout << "Valor de RSI: 0x" << std::hex << rsi_value << std::endl;

    //ADDRINT six_value = PIN_GetContextReg(ctxt, REG_SI);
    //ADDRINT rbp_value = PIN_GetContextReg(ctxt, REG_RBP);
    //ADDRINT rax_value = PIN_GetContextReg(ctxt, REG_RAX);

    //for (size_t i = 0; i < 15; i++)
    //{
    //    ADDRINT result = rbp_value + rax_value * 2;

    //    if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(result)))
    //    {
    //        wchar_t* CharStr = reinterpret_cast<wchar_t*>(result);
    //        std::wstring wstr(CharStr);
    //        wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);

    //    }
    //    result = result + 0x1;
    //}


}


VOID SequenceMatchReadStringCmp2Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT rbp_value = PIN_GetContextReg(ctxt, REG_RBP);
    wchar_t* CharStr = reinterpret_cast<wchar_t*>(rbp_value);
    std::wstring wstr(CharStr);
    wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);
}

VOID SequenceMatchReadStringCmp3Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT rdi_value = PIN_GetContextReg(ctxt, REG_RCX);
    wchar_t* CharStr = reinterpret_cast<wchar_t*>(rdi_value);
    std::wstring wstr(CharStr);
    wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);

    char* CharStr2 = reinterpret_cast<char*>(rdi_value);
    std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
}


// Leitura de depuradores
VOID SequenceMatchReadStringCmp40Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT r_value = PIN_GetContextReg(ctxt, REG_R10) + PIN_GetContextReg(ctxt, REG_RCX) * 1 + 0x1;

    for (size_t i = 0; i < 1; i++)
    {
        std::cout << "ADDR: " << std::hex << r_value << std::dec << std::endl;

        if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(r_value))) {
            wchar_t* CharStr = reinterpret_cast<wchar_t*>(r_value);
            std::wstring wstr(CharStr);
            wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);

            char* CharStr2 = reinterpret_cast<char*>(r_value);
            std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
            r_value = r_value + 0x1;
        }

    }


}


// IMPORTANTE - Contem os valores a serem comparados com os nomes de alguns depuradores
VOID SequenceMatchReadStringCmp41Callback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT r_value = PIN_GetContextReg(ctxt, REG_RBP) + PIN_GetContextReg(ctxt, REG_RCX) * 2 + 0x0;

    for (size_t i = 0; i < 1; i++)
    {
        std::cout << "ADDR: " << std::hex << r_value << std::dec << " T[" << tid << "]" << std::endl;
        std::cout << "TXT: " << r_value << std::endl;

        if (PIN_CheckReadAccess(reinterpret_cast<VOID*>(r_value))) {
            wchar_t* CharStr = reinterpret_cast<wchar_t*>(r_value);
            std::wstring wstr(CharStr);
            wprintf(L" ** RETURN WString --> %ls <-\n", CharStr);

            char* CharStr2 = reinterpret_cast<char*>(r_value);
            std::cout << " ** RETURN StringArg --> " << CharStr2 << std::endl;
            r_value = r_value + 0x1;
        }
       
    }


}



VOID SequenceMatchAntiDebugSameTarget(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    std::deque<InstrumentedInstruction*> instruction_queue = disassembly_seq_detector.GetDisassembledInstructionQueue();

    InstrumentedInstruction* inst = instruction_queue[instruction_queue.size() - 2];
    InstrumentedInstruction* inst2 = instruction_queue[instruction_queue.size() - 1];

    std::string inststr = inst->instructionStr;
    std::string inststr2 = inst2->instructionStr;

    std::string subinst = inststr.substr(2, 15);
    std::string subinst2 = inststr2.substr(3, 15);

    if (subinst == subinst2 && (inst->instAddr + 2) == inst2->instAddr) {  // 2 = tamanho da instrução jz (SHORT JUMP)
        std::cout << "\n[CONTRADEF][ANTIDEBUG] Instruções de Salto com o Mesmo Alvo:" << std::endl;
        std::cout << "    " << std::hex << inst->instAddr << " | " << inststr << std::dec << std::endl;
        std::cout << "    " << std::hex << inst2->instAddr << " | " << inststr2 << std::dec << std::endl;
    }
}


VOID SequenceMatchAntiDebugConstCondition(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    std::deque<InstrumentedInstruction*> instruction_queue = disassembly_seq_detector.GetDisassembledInstructionQueue();

    InstrumentedInstruction* inst = instruction_queue[instruction_queue.size() - 2];
    InstrumentedInstruction* inst2 = instruction_queue[instruction_queue.size() - 1];

    std::string inststr = inst->instructionStr;
    std::string inststr2 = inst2->instructionStr;

    if ((inst->instAddr + 2) == inst2->instAddr) { // 2 = tamanho da instrução xor
        std::cout << "\n[CONTRADEF][ANTIDEBUG] Instruções com Condição Constante:" << std::endl;
        std::cout << "    " << std::hex << inst->instAddr << " | " << inststr << std::dec << std::endl;
        std::cout << "    " << std::hex << inst2->instAddr << " | " << inststr2 << std::dec << std::endl;


    }
}


VOID SequenceMatchPauseCallback(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    OS_PROCESS_ID pid = PIN_GetPid();
    std::cout << "Sequencia de instrucoes identificada, endereco de instrucao " << std::hex << ip << std::dec << ", corresponde a funcao: " << GetFunctionFromAddress(ip, pid).functionName << std::endl;
    PauseAtAddress(ip);
}


// From regval.cpp
static void PrintRegisters(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    static const UINT stRegSize = REG_Size(REG_ST_BASE);
    for (int reg = (int)REG_GR_BASE; reg <= (int)REG_GR_LAST; ++reg)
    {
        // For the integer registers, it is safe to use ADDRINT. But make sure to pass a pointer to it.
        ADDRINT val;
        PIN_GetContextRegval(ctxt, (REG)reg, reinterpret_cast<UINT8*>(&val));
        std::cout << REG_StringShort((REG)reg) << ": 0x" << std::hex << val << std::endl;
    }
    for (int reg = (int)REG_ST_BASE; reg <= (int)REG_ST_LAST; ++reg)
    {
        // For the x87 FPU stack registers, using PINTOOL_REGISTER ensures a large enough buffer.
        PINTOOL_REGISTER val;
        PIN_GetContextRegval(ctxt, (REG)reg, reinterpret_cast<UINT8*>(&val));
        std::cout << REG_StringShort((REG)reg) << ": " << Val2Str(&val, stRegSize) << std::endl;
    }
}


VOID SetZfToOne(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;

    if (zf_before == 0) {

        // Para setar o ZF para 1:
        flags |= (1 << 6); // Seta o bit 6 (ZF) para 1
        std::cout << "APLICANDO Patch NO INDICE -> " << sequence.matchCount << std::endl;

        // Atualizar os flags
        PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        PIN_ExecuteAt(ctxt);
    }

}

VOID SetZfToZero(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    // Obter os flags atuais
    ADDRINT flags = PIN_GetContextReg(ctxt, REG_RFLAGS);

    // Verificar o valor atual do ZF
    BOOL zf_before = (flags & (1 << 6)) != 0;

    if (zf_before == 1) {

        // Para setar o ZF para 0:
        flags &= ~(1 << 6); // Limpa o bit 6 (ZF) para 0
        std::cout << "APLICANDO Patch NO INDICE -> " << sequence.matchCount << std::endl;

        // Atualizar os flags
        PIN_SetContextReg(ctxt, REG_RFLAGS, flags);

        PIN_ExecuteAt(ctxt);
    }

}

// Função que redireciona a execução para o endereço de destino
void JumpToAddress(THREADID tid, ADDRINT ip, CONTEXT* ctxt, SequenceInstructions& sequence)
{
    ADDRINT destAddr = 0; // Definir o valor
    // Redireciona a execução para o endereço destino
    PIN_SetContextReg(ctxt, REG_INST_PTR, destAddr);
    PIN_ExecuteAt(ctxt);
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

VOID TraceFunc(TRACE trace, VOID* v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            InstrumentFuncCall(trace, ins);
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


VOID TraceInst(INS ins, VOID* v)
{
    seq_detector.InstructionTrace(ins, &seq_detector, onCall);
    disassembly_seq_detector.InstructionTrace(ins, &disassembly_seq_detector, onDisassembly);

    std::string disassembledInstr = INS_Disassemble(ins);

    if (KnobDisassembly) {
        disassemblyTraceOut << INS_Address(ins) << " | " << disassembledInstr << std::endl;
    }
    return;


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

void saveAllExternalCalls(std::ofstream& ExternalCallTraceOutFile) {
    callTracker.saveAllExternalCalls(ExternalCallTraceOutFile);
}

int InitInstrumentation(std::string pid)
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
    disassembly_seq_detector.AddInstructionSequence(ad_same_target, SequenceMatchAntiDebugSameTarget);


    // Tecnica antidebug, verificada
    std::vector<std::vector<std::string>> ad_const_condition;
    ad_const_condition.push_back(CreateInstVector("xor eax, eax"));
    ad_const_condition.push_back(CreateInstVector("jz 0x7ff62f10fe35"));
    disassembly_seq_detector.AddInstructionSequence(ad_const_condition, SequenceMatchAntiDebugConstCondition);


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

    seq_detector.Initialize();
    disassembly_seq_detector.Initialize();

    if (KnobDisassembly) {
        string disassemblyFilename = "contradef";
        disassemblyFilename += "." + pid + ".disassembly.cdf";
        disassemblyTraceOut.open(disassemblyFilename.c_str());
        disassemblyTraceOut << hex << right;
        disassemblyTraceOut.setf(ios::showbase);
    }
    
    TRACE_AddInstrumentFunction(TraceFunc, 0);

    INS_AddInstrumentFunction(TraceInst, 0);

    //InitTrace(pid);

    return 0;
}