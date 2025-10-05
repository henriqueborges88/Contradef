#pragma once
#include "pin.H"
#include <vector>
#include <cstdint>

namespace HeapTracker
{
    struct HeapRegion
    {
        ADDRINT base;
        size_t  size;
    };

    /* ------------------------------------------------- */
    /*      inicialização / consulta                      */
    /* ------------------------------------------------- */
    VOID   Init();
    BOOL   IsInTrackedHeap(ADDRINT addr);

    /* ------------------------------------------------- */
    /*      (opcional) obter lista para debug             */
    /* ------------------------------------------------- */
    const std::vector<HeapRegion>& GetTrackedRegions();
} // namespace HeapTracker
