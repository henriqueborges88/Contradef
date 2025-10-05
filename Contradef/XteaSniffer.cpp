/**********************************************************************
 *  Extrai texto em claro de
 *  rotinas XTEA (detectadas por constantes 0x9E3779B9 / 0xC6EF3720)
 *********************************************************************/
#include "XteaSniffer.h"

#include <fstream>
#include <set>
#include <vector>
#include <iomanip>
#include <cstring>
#include <iostream>

namespace XteaSniffer
{
    /*----------------------------------------------------------------------*/
    /*  Configurações                                                       */
    /*----------------------------------------------------------------------*/
    static const UINT32 MAGIC_DELTA = 0x9E3779B9;     // 265 443 5769
    static const size_t MAX_DUMP = 1024;           // bytes mostrados

    /*----------------------------------------------------------------------*/
    /*  Estado global                                                       */
    /*----------------------------------------------------------------------*/
    static std::ofstream        gOut;
    static PIN_LOCK             gLock;               // protege estruturas
    static std::set<ADDRINT>    gTargetAddrs;        // rotinas já confirmadas

    /*----------------------------------------------------------------------*/
    /*  Utilidades                                                          */
    /*----------------------------------------------------------------------*/
    static VOID DumpBuffer(THREADID tid, ADDRINT buf, ADDRINT len, ADDRINT dst, ADDRINT retVal)
    {
        if (len == 0) return;

        size_t toCopy = static_cast<size_t>(len);
        if (toCopy > MAX_DUMP) toCopy = MAX_DUMP;

        void* p = reinterpret_cast<void*>(dst);
        void* p2 = reinterpret_cast<void*>(buf);
        char* r = reinterpret_cast<char*>(retVal);

        if (!PIN_CheckReadAccess(p)) return;

        char local[MAX_DUMP + 1] = { 0 };
        char local2[MAX_DUMP + 1] = { 0 };
        size_t copied = PIN_SafeCopy(local, p, toCopy);
        size_t copied2 = PIN_SafeCopy(local2, p2, toCopy);
        if (copied == 0) return;
        local[copied] = 0;
        local2[copied2] = 0;

        PIN_GetLock(&gLock, 2);
        gOut << "[T" << tid << "] [" << len << "] \"" << local << '"';
        gOut << " -> \"" << local2 << '"';
        gOut << " | \"" << *r << '"';
        gOut << std::endl;
        PIN_ReleaseLock(&gLock);
    }

    /*----------------------------------------------------------------------*/
    /*  Call-back AFTER genérico – roda para TODAS as rotinas               */
    /*----------------------------------------------------------------------*/
    static VOID AfterUniversal(ADDRINT rtnAddr,
        THREADID tid,
        ADDRINT buf,
        ADDRINT len,
        ADDRINT dst,
        ADDRINT retVal)
    {
        if (gTargetAddrs.find(rtnAddr) == gTargetAddrs.end()) return; // não é alvo
        DumpBuffer(tid, buf, len, dst, retVal);
    }

    /*----------------------------------------------------------------------*/
    /*  “Radar” — procura o MAGIC no valor escrito em registradores         */
    /*----------------------------------------------------------------------*/
    static VOID CheckReg(THREADID tid, ADDRINT val, ADDRINT ip)
    {
        if (static_cast<UINT32>(val) != MAGIC_DELTA) return;

        /* Protegemos a região crítica pois várias threads podem detectar   */
        PIN_GetLock(&gLock, 1);

        /* Descobre a rotina que contém essa instrução                      */
        PIN_LockClient();
        RTN rtn = RTN_FindByAddress(ip);
        PIN_UnlockClient();

        if (RTN_Valid(rtn))
        {
            ADDRINT addr = RTN_Address(rtn);

            if (gTargetAddrs.insert(addr).second)
            {
                gOut << "[XTEA-Sniffer] rotina candidata em 0x"
                    << std::hex << addr << std::dec
                    << "  (encontrado delta 0x9E3779B9)" << std::endl;
            }
        }

        PIN_ReleaseLock(&gLock);
    }

    /*----------------------------------------------------------------------*/
    /*  Instrumentação de rotinas – coloca AFTER universal                  */
    /*----------------------------------------------------------------------*/
    static VOID Routine(RTN rtn, VOID*)
    {
        ADDRINT rtnAddr = RTN_Address(rtn);

        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)AfterUniversal,
            IARG_ADDRINT, rtnAddr,
            IARG_THREAD_ID,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,   // buf  (char*)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,   // len  (size_t)
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,   // dst  (char*) ou NULL
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_END);
        RTN_Close(rtn);
    }

    /*----------------------------------------------------------------------*/
    /*  Instrumentação de instruções – “radar” do delta                     */
    /*----------------------------------------------------------------------*/
    static VOID Trace(TRACE trace, VOID*)
    {
        for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
        {
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
            {
                if (!INS_IsValidForIpointAfter(ins)) continue;

                for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); ++i)
                {
                    REG r = REG_FullRegName(INS_RegW(ins, i));
                    if (!REG_is_gr(r)) continue;

                    INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR)CheckReg,
                        IARG_THREAD_ID,
                        IARG_REG_VALUE, r,
                        IARG_INST_PTR,
                        IARG_END);
                }
            }
        }
    }

    static VOID Fini(INT32, VOID*)
    {
        if (gOut.is_open()) gOut.close();
    }

    int InitXteaSniffer(std::string pid, std::string filename)
    {
        filename += "." + pid + ".xtea.cdf";
        gOut.open(filename.c_str(), std::ios::out | std::ios::trunc);

        if (!gOut)
        {
            std::cout << "XteaSniffer: não foi possível criar '" << filename << "'\n";
            return -1;
        }

        PIN_InitLock(&gLock);

        TRACE_AddInstrumentFunction(Trace, 0);
        RTN_AddInstrumentFunction(Routine, 0);
        PIN_AddFiniFunction(Fini, 0);

        gOut << "[XTEA-Sniffer] inicializado – procurando o delta 0x9E3779B9\n";
        return 0;
    }
} // namespace XteaSniffer