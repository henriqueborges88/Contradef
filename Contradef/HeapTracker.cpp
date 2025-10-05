#include "HeapTracker.h"
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>

/* ------------------------------------------------------------------ */
/*               dados globais                                         */
/* ------------------------------------------------------------------ */
namespace HeapTracker
{
    // regiões efectivamente registadas
    static std::vector<HeapRegion>      gRegions;
    static PIN_MUTEX                    gMutex;

    // ----------------  suporte TLS  -----------------
    /* Guardamos, por thread, o último tamanho visto em BEFORE
       e consumimos em AFTER.  Se preferir suportar recursão,
       troque por std::stack<size_t>.                                                */
    struct PendingAlloc
    {
        size_t  size = 0;
    };
    static TLS_KEY gTlsKey;

    static PendingAlloc* Tls()
    {
        PendingAlloc* p = reinterpret_cast<PendingAlloc*>(PIN_GetThreadData(gTlsKey, PIN_ThreadId()));
        if (!p)
        {
            p = new PendingAlloc;
            PIN_SetThreadData(gTlsKey, p, PIN_ThreadId());
        }
        return p;
    }

    /* ------------------------------------------------------------------ */
    /*               helpers                                              */
    /* ------------------------------------------------------------------ */
    static inline VOID RecordRegion(ADDRINT addr, size_t size)
    {
        if (addr == 0 || size == 0) return;
        PIN_MutexLock(&gMutex);
        gRegions.push_back({ addr, size });
        PIN_MutexUnlock(&gMutex);
    }

    BOOL IsInTrackedHeap(ADDRINT addr)
    {
        PIN_MutexLock(&gMutex);
        for (const auto& r : gRegions)
            if (addr >= r.base && addr < r.base + r.size)
            {
                PIN_MutexUnlock(&gMutex); return TRUE;
            }
        PIN_MutexUnlock(&gMutex);
        return FALSE;
    }

    const std::vector<HeapRegion>& GetTrackedRegions() { return gRegions; }

    /* ------------------------------------------------------------------ */
    /*               BEFORE  capturam tamanho                             */
    /* ------------------------------------------------------------------ */
    static VOID BeforeMalloc(size_t size) { Tls()->size = size; }
    static VOID BeforeHeapAlloc(ADDRINT, ADDRINT, size_t size) { Tls()->size = size; }
    static VOID BeforeRtlAllocateHeap(ADDRINT, ADDRINT, size_t size) { Tls()->size = size; }
    static VOID BeforeVirtualAlloc(ADDRINT, size_t size, ADDRINT, ADDRINT) { Tls()->size = size; }
    static VOID BeforeGlobalAlloc(ADDRINT, size_t size) { Tls()->size = size; }
    static VOID BeforeLocalAlloc(ADDRINT, size_t size) { Tls()->size = size; }

    /* ------------------------------------------------------------------ */
    /*               AFTER  registam ponteiro + tamanho                   */
    /* ------------------------------------------------------------------ */
    static VOID AfterGeneric(ADDRINT retPtr)
    {
        size_t sz = Tls()->size;
        Tls()->size = 0;                    // limpa
        if (sz > 0 && sz < (1ULL << 40))    // 1 TiB como limite sanidade
            RecordRegion(retPtr, sz);
    }

    /* ------------------------------------------------------------------ */
    /*               Instrumentação por imagem                            */
    /* ------------------------------------------------------------------ */
    static VOID HookRoutine(IMG img,
        const char* name,
        AFUNPTR      before,
        UINT32       sizeIdx,    // apenas para ASSERT / debug
        bool         isCdecl = false)
    {
        RTN rtn = RTN_FindByName(img, name);
        if (!RTN_Valid(rtn)) return;

        RTN_Open(rtn);

        // BEFORE  -------------------------------------------------
        if (before)
        {
            RTN_InsertCall(rtn, IPOINT_BEFORE, before,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 2,
                IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
                IARG_END);
        }

        // AFTER  --------------------------------------------------
        RTN_InsertCall(rtn, isCdecl ? IPOINT_AFTER : IPOINT_AFTER,
            (AFUNPTR)AfterGeneric,
            IARG_FUNCRET_EXITPOINT_VALUE,
            IARG_END);

        RTN_Close(rtn);
    }

    VOID ImageLoad(IMG img, VOID*)
    {
        /*                 nome da API      BEFORE-cb         parâmetro Size  */
        HookRoutine(img, "malloc", (AFUNPTR)BeforeMalloc, 0);
        HookRoutine(img, "HeapAlloc", (AFUNPTR)BeforeHeapAlloc, 2);
        HookRoutine(img, "RtlAllocateHeap", (AFUNPTR)BeforeRtlAllocateHeap, 2);
        HookRoutine(img, "VirtualAlloc", (AFUNPTR)BeforeVirtualAlloc, 1);
        HookRoutine(img, "GlobalAlloc", (AFUNPTR)BeforeGlobalAlloc, 1);
        HookRoutine(img, "LocalAlloc", (AFUNPTR)BeforeLocalAlloc, 1);
    }

    /* ------------------------------------------------------------------ */
    /*               inicialização pública                                 */
    /* ------------------------------------------------------------------ */
    VOID Init()
    {
        PIN_MutexInit(&gMutex);
        gTlsKey = PIN_CreateThreadDataKey(0);

        IMG_AddInstrumentFunction(ImageLoad, nullptr);
    }
} // namespace HeapTracker
