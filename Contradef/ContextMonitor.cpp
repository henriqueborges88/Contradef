#include "ContextMonitor.h"
#include "InstrumentationUtils.h"
#include "Utils.h"
#include <string>
#include <iostream>
#include <fstream>
#include "InitParams.h"

// Arquivo de saída para o nosso log.
std::ofstream LogFile;

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

int InitContextMonitor() {
    std::string pid = decstr(WindowsAPI::getpid());
    std::string logsName = KnobOutputFile.Value();
    std::string excptFileName = logsName + "." + pid + ".Exceptions.cdf";
    LogFile.open(excptFileName.c_str(), std::ios::out | std::ios::binary);
    LogFile << "Pintool started. Waiting for exceptions..." << std::endl;
    PIN_AddContextChangeFunction(OnContextChange, 0);
    return 1;
}