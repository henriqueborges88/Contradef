#pragma once
#ifndef TRACE_FCN_CALL_M2_H
#define TRACE_FCN_CALL_M2_H

#include <fstream>
#include <stack>
#include <pin.H>

namespace TraceFcnCallM2
{
    /* ---------- interface pública ---------- */
    INT32  Init(std::string pid, std::string filename);
    void   Enable();
    void   Disable();

    /* ---------- callbacks de PIN ---------- */
    VOID   Trace(TRACE trace, VOID* v);

} // namespace TraceFcnCallM2

#endif // TRACE_FCN_CALL_M2_H
