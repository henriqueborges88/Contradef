#include "DynExecTracker.h"
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include "Utils.h"

namespace DynExecTracker
{
    // opções
    static Options       gCfg;

    // página
    static size_t        gPageSize = 0;
    static ADDRINT       gPageMask = 0;

    static inline ADDRINT PageOf(ADDRINT a) { return (a & ~((ADDRINT)gPageSize - 1)); }

    // índices
    static PIN_MUTEX     gIdxMx;
    static ::stlport::tr1::unordered_map<ADDRINT, uint32_t> gTrackedRef;  // page -> refcount (heap/VA/MVF)
    static ::stlport::tr1::unordered_map<ADDRINT, DWORD>    gProt;        // page -> proteção (cache local)
    static ::stlport::tr1::unordered_set<ADDRINT>           gRWtoRX;      // páginas que sofreram RW->RX
    static ::stlport::tr1::unordered_set<ADDRINT>           gSeenExec;    // 1a execução em página
    static ::stlport::tr1::unordered_set<ADDRINT>           gWritten;     // páginas que receberam write

    // TLS para parear BEFORE/AFTER
    struct Pending
    {
        std::stack<size_t> sizes;   // malloc/HeapAlloc/VirtualAlloc/VirtualAllocEx/VirtualAlloc2/MapViewOfFile*

        struct Vp { ADDRINT addr; size_t size; DWORD newp; };
        std::stack<Vp>     vps;     // VirtualProtect / NtProtectVirtualMemory

        std::stack<ADDRINT> frees;  // ponteiros de free/HeapFree/GlobalFree/LocalFree

        // VirtualAllocEx
        struct VAEx { ADDRINT hProcess; size_t size; DWORD protect; };
        std::stack<VAEx>    vaex;

        // VirtualAlloc2
        struct VA2 { ADDRINT hProcess; size_t size; DWORD protect; };
        std::stack<VA2>     va2;

        // MapViewOfFile / MapViewOfFileEx (só precisamos do tamanho solicitado)
        struct MVF { size_t size; };
        std::stack<MVF>     mvf;
    };
    static TLS_KEY gTls = INVALID_TLS_KEY;

    static inline Pending* TLS()
    {
        THREADID tid = PIN_ThreadId();
        auto* p = reinterpret_cast<Pending*>(PIN_GetThreadData(gTls, tid));
        if (!p) { p = new Pending(); PIN_SetThreadData(gTls, p, tid); }
        return p;
    }

    /* =====================================================================================
       Helpers de SO
       ===================================================================================== */
    static inline bool QueryPageInfo(ADDRINT a, DWORD& outProtect, DWORD& outType)
    {
        using namespace WindowsAPI;
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T n = VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi));
        if (!n) return false;
        outProtect = mbi.Protect;
        outType = mbi.Type; // MEM_PRIVATE / MEM_IMAGE / MEM_MAPPED
        return true;
    }

    static inline bool ProtHasExec(DWORD p)
    {
        return (p & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }
    static inline bool ProtHasWrite(DWORD p)
    {
        return (p & (PAGE_READWRITE | PAGE_WRITECOPY |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    // Best-effort: verificar se o handle pertence ao processo atual
    static inline bool IsCurrentProcessHandle(ADDRINT h)
    {
        using namespace WindowsAPI;
        HANDLE hp = reinterpret_cast<HANDLE>(h);
        if (hp == GetCurrentProcess()) return true; // pseudohandle

        // tenta GetProcessId dinamicamente
        typedef DWORD(WINAPI* PFN_GetProcessId)(HANDLE);
        HMODULE hK32 = GetModuleHandleA("kernel32.dll");
        PFN_GetProcessId pGetProcessId =
            hK32 ? (PFN_GetProcessId)GetProcAddress(hK32, "GetProcessId") : nullptr;
        if (!pGetProcessId) return false;

        DWORD pid = pGetProcessId(hp);
        return (pid != 0 && pid == GetCurrentProcessId());
    }

    /* =====================================================================================
       Índice
       ===================================================================================== */
    static VOID IndexAddRegion(ADDRINT base, size_t size, DWORD protHint = 0)
    {
        if (!base || !size) return;
        const ADDRINT end = base + size - 1;
        PIN_MutexLock(&gIdxMx);
        for (ADDRINT p = PageOf(base); p <= PageOf(end); p += gPageSize)
        {
            gTrackedRef[p] += 1;
            if (protHint) gProt[p] = protHint;
        }
        PIN_MutexUnlock(&gIdxMx);
    }

    static VOID IndexRemoveRegion(ADDRINT base, size_t size)
    {
        if (!base || !size) return;
        const ADDRINT end = base + size - 1;
        PIN_MutexLock(&gIdxMx);
        for (ADDRINT p = PageOf(base); p <= PageOf(end); p += gPageSize)
        {
            auto it = gTrackedRef.find(p);
            if (it != gTrackedRef.end())
            {
                if (it->second > 1) it->second -= 1;
                else {
                    gTrackedRef.erase(it);
                    gProt.erase(p);
                    gRWtoRX.erase(p);
                    gSeenExec.erase(p);
                    gWritten.erase(p);
                }
            }
        }
        PIN_MutexUnlock(&gIdxMx);
    }

    static inline DWORD GetProtCached(ADDRINT p, bool* has = nullptr)
    {
        PIN_MutexLock(&gIdxMx);
        auto it = gProt.find(p);
        bool found = (it != gProt.end());
        DWORD v = found ? it->second : 0;
        PIN_MutexUnlock(&gIdxMx);
        if (has) *has = found;
        return v;
    }
    static inline VOID SetProtCached(ADDRINT p, DWORD v)
    {
        PIN_MutexLock(&gIdxMx);
        gProt[p] = v;
        PIN_MutexUnlock(&gIdxMx);
    }

    /* =====================================================================================
       RW->RX
       ===================================================================================== */
    static inline VOID MarkRWtoRXRange(ADDRINT addr, size_t size, DWORD newProt)
    {
        if (!size) return;
        const ADDRINT end = addr + size - 1;
        for (ADDRINT p = PageOf(addr); p <= PageOf(end); p += gPageSize)
        {
            bool had = false; DWORD oldProt = GetProtCached(p, &had);
            if (!had) {
                DWORD qProt = 0, qType = 0;
                if (QueryPageInfo(p, qProt, qType)) oldProt = qProt;
            }
            if (ProtHasExec(newProt) && ProtHasWrite(oldProt) && !ProtHasExec(oldProt))
            {
                PIN_MutexLock(&gIdxMx);
                gRWtoRX.insert(p);
                PIN_MutexUnlock(&gIdxMx);
            }
            SetProtCached(p, newProt);
        }
    }

    /* =====================================================================================
       BEFORE/AFTER alocadores
       ===================================================================================== */
    static VOID BeforeSize_1(size_t sz) { TLS()->sizes.push(sz); } // malloc, GlobalAlloc(size), LocalAlloc(size)

    static VOID AfterAllocCommon(ADDRINT ret)
    {
        size_t sz = 0;
        if (!TLS()->sizes.empty()) { sz = TLS()->sizes.top(); TLS()->sizes.pop(); }
        if (ret && sz)
        {
            DWORD prot = 0, type = 0; QueryPageInfo(ret, prot, type);
            IndexAddRegion(ret, sz, prot);
        }
    }

    // HeapAlloc(HANDLE, DWORD, SIZE_T)
    static VOID Before_HeapAlloc(ADDRINT, DWORD, size_t sz) { TLS()->sizes.push(sz); }
    // RtlAllocateHeap(HANDLE, ULONG, SIZE_T)
    static VOID Before_RtlAllocateHeap(ADDRINT, DWORD, size_t sz) { TLS()->sizes.push(sz); }
    // VirtualAlloc(LPVOID,SIZE_T,DWORD,DWORD)
    static VOID Before_VirtualAlloc(ADDRINT, size_t sz, DWORD, DWORD) { TLS()->sizes.push(sz); }

    // frees
    static VOID Before_free(ADDRINT p) { TLS()->frees.push(p); }
    static VOID Before_HeapFree(ADDRINT, ADDRINT, ADDRINT p) { TLS()->frees.push(p); }
    static VOID Before_GlobalFree(ADDRINT p) { TLS()->frees.push(p); }
    static VOID Before_LocalFree(ADDRINT p) { TLS()->frees.push(p); }

    static VOID After_FreeCommon()
    {
        using namespace WindowsAPI;
        if (TLS()->frees.empty()) return;
        ADDRINT p = TLS()->frees.top(); TLS()->frees.pop();
        if (!p) return;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)p, &mbi, sizeof(mbi)))
            IndexRemoveRegion((ADDRINT)mbi.BaseAddress, (size_t)mbi.RegionSize);
    }

    // VirtualFree(LPVOID,SIZE_T,DWORD)
    static VOID Before_VirtualFree(ADDRINT p, size_t sz, DWORD)
    {
        TLS()->frees.push(p);
        if (p && sz == 0)
        {
            using namespace WindowsAPI;
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery((LPCVOID)p, &mbi, sizeof(mbi)))
                sz = (size_t)mbi.RegionSize;
        }
        if (p && sz) IndexRemoveRegion(p, sz);
    }

    /* =====================================================================================
       VirtualProtect / NtProtectVirtualMemory
       ===================================================================================== */
       // VirtualProtect(LPVOID,SIZE_T,DWORD,PDWORD)
    static VOID Before_VirtualProtect(ADDRINT addr, size_t sz, DWORD newp, ADDRINT)
    {
        TLS()->vps.push({ addr, sz, newp });
    }
    static VOID After_VirtualProtect(ADDRINT ret)
    {
        if (TLS()->vps.empty()) return;
        auto vp = TLS()->vps.top(); TLS()->vps.pop();
        if (ret) MarkRWtoRXRange(vp.addr, vp.size, vp.newp);
    }

    // NtProtectVirtualMemory(HANDLE,PVOID*,PSIZE_T,ULONG,PULONG)
    static VOID Before_NtProtect(ADDRINT /*hProc*/, ADDRINT pAddr, ADDRINT pSize, DWORD newp, ADDRINT)
    {
        using namespace WindowsAPI;
        ADDRINT base = 0; SIZE_T bytes = 0;
        PIN_SafeCopy(&base, (VOID*)pAddr, sizeof(base));
        PIN_SafeCopy(&bytes, (VOID*)pSize, sizeof(bytes));
        TLS()->vps.push({ base, (size_t)bytes, newp });
    }
    static VOID After_NtProtect(ADDRINT status)
    {
        if (TLS()->vps.empty()) return;
        auto vp = TLS()->vps.top(); TLS()->vps.pop();
        if ((LONG)status >= 0) MarkRWtoRXRange(vp.addr, vp.size, vp.newp);
    }

    /* =====================================================================================
       VirtualAllocEx / VirtualAlloc2
       ===================================================================================== */
       // BEFORE: VirtualAllocEx(HANDLE, LPVOID, SIZE_T, DWORD, DWORD)
    static VOID Before_VirtualAllocEx(ADDRINT hProc, ADDRINT /*lpAddress*/, size_t sz, DWORD /*flAlloc*/, DWORD prot)
    {
        TLS()->vaex.push({ hProc, sz, prot });
    }
    // AFTER: base
    static VOID After_VirtualAllocEx(ADDRINT ret)
    {
        if (TLS()->vaex.empty()) return;
        auto a = TLS()->vaex.top(); TLS()->vaex.pop();
        if (!ret || !a.size) return;
        if (!IsCurrentProcessHandle(a.hProcess)) return;  // ignore alocação remota
        IndexAddRegion(ret, a.size, a.protect);
    }

    // BEFORE: VirtualAlloc2(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG)
    static VOID Before_VirtualAlloc2(ADDRINT hProc, ADDRINT /*Base*/, size_t sz, DWORD /*AllocType*/, DWORD prot, ADDRINT /*Ext*/, DWORD /*ExtCount*/)
    {
        TLS()->va2.push({ hProc, sz, prot });
    }
    // AFTER: base
    static VOID After_VirtualAlloc2(ADDRINT ret)
    {
        if (TLS()->va2.empty()) return;
        auto a = TLS()->va2.top(); TLS()->va2.pop();
        if (!ret || !a.size) return;
        if (!IsCurrentProcessHandle(a.hProcess)) return;  // ignore alocação remota
        IndexAddRegion(ret, a.size, a.protect);
    }

    /* =====================================================================================
       MapViewOfFile / MapViewOfFileEx
       ===================================================================================== */
       // BEFORE: MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T)
    static VOID Before_MapViewOfFile(ADDRINT /*hMap*/, DWORD /*access*/, DWORD /*hi*/, DWORD /*lo*/, size_t nBytes)
    {
        TLS()->mvf.push({ nBytes });
    }
    // BEFORE: MapViewOfFileEx(HANDLE, DWORD, DWORD, DWORD, SIZE_T, LPVOID)
    static VOID Before_MapViewOfFileEx(ADDRINT /*hMap*/, DWORD /*access*/, DWORD /*hi*/, DWORD /*lo*/, size_t nBytes, ADDRINT /*base*/)
    {
        TLS()->mvf.push({ nBytes });
    }
    // AFTER comum: retorno é a base mapeada (sempre no processo atual)
    static VOID After_MapViewCommon(ADDRINT ret)
    {
        if (TLS()->mvf.empty()) return;
        auto m = TLS()->mvf.top(); TLS()->mvf.pop();
        if (!ret) return;

        using namespace WindowsAPI;
        SIZE_T sz = m.size;
        DWORD prot = 0, type = 0;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)ret, &mbi, sizeof(mbi))) {
            prot = mbi.Protect;
            if (sz == 0) sz = (SIZE_T)mbi.RegionSize; // mapear tudo
        }
        if (sz) IndexAddRegion(ret, (size_t)sz, prot); // normalmente MEM_MAPPED
    }

    /* =====================================================================================
       Execução e writes
       ===================================================================================== */
    static inline bool IsMemPrivate(ADDRINT ip)
    {
        DWORD prot = 0, type = 0;
        if (!QueryPageInfo(ip, prot, type)) return false;
        return type == MEM_PRIVATE;
    }

    static VOID OnExec(ADDRINT ip)
    {
        ADDRINT pg = PageOf(ip);
        bool first = false;
        PIN_MutexLock(&gIdxMx);
        first = gSeenExec.insert(pg).second;
        PIN_MutexUnlock(&gIdxMx);

        if (!first) return;
        // cache proteção
        DWORD prot = 0, type = 0;
        if (QueryPageInfo(ip, prot, type))
            SetProtCached(pg, prot);
    }

    static VOID OnWrite(ADDRINT ea)
    {
        if (!gCfg.trackWrites) return;
        ADDRINT pg = PageOf(ea);
        PIN_MutexLock(&gIdxMx);
        gWritten.insert(pg);
        PIN_MutexUnlock(&gIdxMx);
    }

    /* =====================================================================================
       Hooks de imagem
       ===================================================================================== */
    static VOID Hook4(IMG img, const char* name, AFUNPTR before, AFUNPTR after)
    {
        // helper original (até 4 args no BEFORE)
        RTN rtn = RTN_FindByName(img, name);
        if (!RTN_Valid(rtn)) return;
        RTN_Open(rtn);
        if (before) RTN_InsertCall(rtn, IPOINT_BEFORE, before,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
            IARG_END);
        if (after)  RTN_InsertCall(rtn, IPOINT_AFTER, after,
            IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
        RTN_Close(rtn);
    }

    static VOID ImageLoad(IMG img, VOID*)
    {
        // Alocações usuais
        Hook4(img, "malloc", (AFUNPTR)BeforeSize_1, (AFUNPTR)AfterAllocCommon);
        Hook4(img, "HeapAlloc", (AFUNPTR)Before_HeapAlloc, (AFUNPTR)AfterAllocCommon);
        Hook4(img, "RtlAllocateHeap", (AFUNPTR)Before_RtlAllocateHeap, (AFUNPTR)AfterAllocCommon);
        Hook4(img, "GlobalAlloc", (AFUNPTR)BeforeSize_1, (AFUNPTR)AfterAllocCommon);
        Hook4(img, "LocalAlloc", (AFUNPTR)BeforeSize_1, (AFUNPTR)AfterAllocCommon);
        Hook4(img, "VirtualAlloc", (AFUNPTR)Before_VirtualAlloc, (AFUNPTR)AfterAllocCommon);

        // Liberações
        Hook4(img, "free", (AFUNPTR)Before_free, (AFUNPTR)After_FreeCommon);
        Hook4(img, "HeapFree", (AFUNPTR)Before_HeapFree, (AFUNPTR)After_FreeCommon);
        Hook4(img, "GlobalFree", (AFUNPTR)Before_GlobalFree, (AFUNPTR)After_FreeCommon);
        Hook4(img, "LocalFree", (AFUNPTR)Before_LocalFree, (AFUNPTR)After_FreeCommon);

        // VirtualFree requer BEFORE custom (3 args) e sem AFTER
        {
            RTN rtn = RTN_FindByName(img, "VirtualFree");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_VirtualFree,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // LPVOID
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // SIZE_T
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // DWORD
                    IARG_END);
                RTN_Close(rtn);
            }
        }

        // Proteção
        Hook4(img, "VirtualProtect", (AFUNPTR)Before_VirtualProtect, (AFUNPTR)After_VirtualProtect);

        if (gCfg.hookNtProtect) {
            RTN rtn = RTN_FindByName(img, "NtProtectVirtualMemory");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_NtProtect,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // HANDLE hProcess
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // PVOID*  Base
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // PSIZE_T Size
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // ULONG   NewProt
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // PULONG  OldProt
                    IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)After_NtProtect,
                    IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
                RTN_Close(rtn);
            }
        }

        // ===== Hooks específicos que usam >4 argumentos =====

        // VirtualAllocEx(HANDLE, LPVOID, SIZE_T, DWORD, DWORD)
        {
            RTN rtn = RTN_FindByName(img, "VirtualAllocEx");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_VirtualAllocEx,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // lpAddress
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // dwSize
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // flAllocationType
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // flProtect
                    IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)After_VirtualAllocEx,
                    IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
                RTN_Close(rtn);
            }
        }

        // VirtualAlloc2(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG)
        {
            RTN rtn = RTN_FindByName(img, "VirtualAlloc2");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_VirtualAlloc2,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hProcess
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // BaseAddress
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // Size
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // AllocationType
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // PageProtection
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // ExtendedParameters
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 6, // ParameterCount
                    IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)After_VirtualAlloc2,
                    IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
                RTN_Close(rtn);
            }
        }

        // MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T)
        {
            RTN rtn = RTN_FindByName(img, "MapViewOfFile");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_MapViewOfFile,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hMap
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // access
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hi
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lo
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // nBytes
                    IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)After_MapViewCommon,
                    IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
                RTN_Close(rtn);
            }
        }

        // MapViewOfFileEx(HANDLE, DWORD, DWORD, DWORD, SIZE_T, LPVOID)
        {
            RTN rtn = RTN_FindByName(img, "MapViewOfFileEx");
            if (RTN_Valid(rtn)) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)Before_MapViewOfFileEx,
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // hMap
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // access
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // hi
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // lo
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 4, // nBytes
                    IARG_FUNCARG_ENTRYPOINT_VALUE, 5, // base
                    IARG_END);
                RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)After_MapViewCommon,
                    IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
                RTN_Close(rtn);
            }
        }
    }

    /* =====================================================================================
       Instrumentação de instruções/trace
       ===================================================================================== */
    static VOID InsInstrument(INS ins, VOID*)
    {
        if (INS_IsMemoryWrite(ins))
        {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, AFUNPTR(OnWrite),
                IARG_MEMORYWRITE_EA, IARG_END);
        }
    }

    static VOID TraceInstrument(TRACE t, VOID*)
    {
        for (BBL b = TRACE_BblHead(t); BBL_Valid(b); b = BBL_Next(b))
        {
            ADDRINT ip = BBL_Address(b);
            BBL_InsertCall(b, IPOINT_BEFORE, AFUNPTR(OnExec),
                IARG_ADDRINT, ip, IARG_END);
        }
    }

    /* =====================================================================================
       API pública
       ===================================================================================== */
    VOID Configure(const Options& cfg) { gCfg = cfg; }

    static VOID ThreadFini(THREADID tid, const CONTEXT*, INT32, VOID*)
    {
        auto* p = reinterpret_cast<Pending*>(PIN_GetThreadData(gTls, tid));
        delete p;
    }

    VOID Init()
    {
        // Configuração padrão (ajuste externamente via Configure se quiser)
        Options opt;
        opt.onlyPrivate = true;   // considerar só MEM_PRIVATE (mas ver exceções abaixo)
        opt.trackWrites = false;  // marcar páginas escritas
        opt.hookNtProtect = true;   // hook NtProtectVirtualMemory
        Configure(opt);

        // Init
        WindowsAPI::SYSTEM_INFO si; WindowsAPI::GetSystemInfo(&si);
        gPageSize = si.dwPageSize ? si.dwPageSize : 0x1000;
        gPageMask = ~((ADDRINT)gPageSize - 1);
        PIN_MutexInit(&gIdxMx);
        gTls = PIN_CreateThreadDataKey(nullptr);

        IMG_AddInstrumentFunction(ImageLoad, nullptr);
        INS_AddInstrumentFunction(InsInstrument, nullptr);
        TRACE_AddInstrumentFunction(TraceInstrument, nullptr);
        PIN_AddThreadFiniFunction(ThreadFini, nullptr);
    }

    BOOL IsIpDynamic(ADDRINT ip)
    {
        PIN_LockClient();
        bool inImg = IMG_Valid(IMG_FindByAddress(ip));
        PIN_UnlockClient();
        if (inImg) return FALSE; // imagem “oficial”

        DWORD prot = 0, type = 0;
        if (!QueryPageInfo(ip, prot, type)) return FALSE;

        // Caso 1: mapeado como IMAGE mas não pertence a IMG "oficial" => loader reflexivo
        if (type == MEM_IMAGE && ProtHasExec(prot)) return TRUE;

        // Caso 2: MEM_MAPPED executável (MapViewOfFile/Ex) => trate como dinâmico
        if (type == MEM_MAPPED && ProtHasExec(prot)) return TRUE;

        // Se o usuário quis considerar somente MEM_PRIVATE, respeite:
        if (gCfg.onlyPrivate && type != MEM_PRIVATE) return FALSE;
        if (!ProtHasExec(prot)) return FALSE;

        ADDRINT pg = PageOf(ip);
        bool rw2rx, wrote, tracked;
        PIN_MutexLock(&gIdxMx);
        rw2rx = (gRWtoRX.count(pg) != 0);
        wrote = (gWritten.count(pg) != 0);
        tracked = (gTrackedRef.count(pg) != 0);
        PIN_MutexUnlock(&gIdxMx);

        if (rw2rx) return TRUE;
        if (gCfg.trackWrites && wrote) return TRUE;
        if (tracked) return TRUE;
        return FALSE;
    }

} // namespace DynExecTracker
