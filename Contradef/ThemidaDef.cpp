#include "ThemidaDef.h"
#include "pin.H"
#include <string>
#include <iostream>
#include <fstream>

// Anti-VM
// Exception Trap bit
static VOID ClearTrapOnPopf(THREADID tid, const CONTEXT* cctx, ADDRINT memea, UINT32 widthBytes)
{
    // widthBytes: 2 (POPF 16b), 4 (POPF/POPFD 32b), 8 (POPFQ 64b)
    if (widthBytes == 8) {
        UINT64 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 8);
        if (v & 0x100ULL) {
            v &= ~0x100ULL;                 // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 8);
        }
    }
    else if (widthBytes == 4) {
        UINT32 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 4);
        if (v & 0x100U) {
            v &= ~0x100U;                   // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 4);

        }
    }
    else if (widthBytes == 2) {
        UINT16 v = 0;
        PIN_SafeCopy(&v, (VOID*)memea, 2);
        if (v & 0x100U) {
            v &= (UINT16)~0x100U;           // limpa TF
            PIN_SafeCopy((VOID*)memea, &v, 2);

        }
    }
}
// End Anti-VM Exception Trap

// Anti-DBI
// Invalid function call exception
static VOID SkipThisCall(ADDRINT nextIp, CONTEXT* c) {
    CONTEXT w; PIN_SaveContext(c, &w);
    PIN_SetContextReg(&w, REG_INST_PTR, nextIp);  // vai para a próxima inst.
    PIN_ExecuteAt(&w);                            // não retorna
}
// End Invalid function call exception


static VOID InstrThemidaCB(INS ins, VOID*)
{
    // Anti-DBI - Invalid function
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

    // Anti-VM - Exception Trap bit
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


int InitThemidaDef() {
    INS_AddInstrumentFunction(InstrThemidaCB, 0);
    return 1;
}