#pragma once
/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

 /*! @file
  *  This file contains declarations for a tool that generates instruction traces with values.
  *  It is designed to help debugging.
  */

#ifndef DEBUGGING_TRACER_H
#define DEBUGGING_TRACER_H

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <stack>
#include "utils.h"
#include <unistd.h>
#include "pin.H"
#include "instlib.H"
#include "control_manager.H"
#include "regvalue_utils.h"
#include "Instrumentation.h"

using namespace CONTROLLER;
using namespace INSTLIB;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

extern KNOB< std::string > KnobTraceOutputFile;
extern KNOB< BOOL > KnobPid;
extern KNOB< THREADID > KnobWatchThread;
extern KNOB< BOOL > KnobFlush;
extern KNOB< BOOL > KnobSymbols;
extern KNOB< BOOL > KnobLines;
extern KNOB< BOOL > KnobTraceInstructions;
extern KNOB< BOOL > KnobTraceCalls;
extern KNOB< BOOL > KnobTraceMemory;
extern KNOB< BOOL > KnobSilent;
extern KNOB< BOOL > KnobEarlyOut;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

extern std::ofstream out;
extern INT32 enabled;
extern FILTER filter;
extern ICOUNT icount;
extern const UINT32 MaxEmitArgs;
extern AFUNPTR emitFuns[];

/* ===================================================================== */
/* Function Declarations */
/* ===================================================================== */

BOOL Emit(THREADID threadid);
VOID Flush();
VOID Handler(EVENT_TYPE ev, VOID*, CONTEXT* ctxt, VOID*, THREADID, bool bcast);
VOID EmitNoValues(THREADID threadid, std::string* str);
VOID Emit1Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val);
VOID Emit2Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val);
VOID Emit3Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val, std::string* reg3str, ADDRINT reg3val);
VOID Emit4Values(THREADID threadid, std::string* str, std::string* reg1str, ADDRINT reg1val, std::string* reg2str, ADDRINT reg2val, std::string* reg3str, ADDRINT reg3val, std::string* reg4str, ADDRINT reg4val);
VOID EmitXMM(THREADID threadid, UINT32 regno, PINTOOL_REGISTER* xmm);
VOID AddXMMEmit(INS ins, IPOINT point, REG xmm_dst);
VOID AddEmit(INS ins, IPOINT point, std::string& traceString, UINT32 regCount, REG regs[]);
VOID CaptureWriteEa(THREADID threadid, VOID* addr);
VOID ShowN(UINT32 n, VOID* ea);
VOID EmitWrite(THREADID threadid, UINT32 size);
VOID EmitRead(THREADID threadid, VOID* ea, UINT32 size);
VOID Indent();
VOID EmitICount();
VOID EmitDirectCall(THREADID threadid, ADDRINT instAddress, std::string* str, INT32 tailCall, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6);
std::string FormatAddress(ADDRINT address, RTN rtn);
VOID EmitIndirectCall(THREADID threadid, ADDRINT instAddress, std::string* str, ADDRINT target, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6);
VOID EmitReturn(THREADID threadid, ADDRINT instAddress, string* str, ADDRINT retAddr, ADDRINT ret0);
VOID CallTrace(TRACE trace, INS ins);
VOID InstructionTrace(TRACE trace, INS ins);
VOID MemoryTrace(INS ins);
VOID Trace(TRACE trace, VOID* v);
VOID OnSig(THREADID threadIndex, CONTEXT_CHANGE_REASON reason, const CONTEXT* ctxtFrom, CONTEXT* ctxtTo, INT32 sig, VOID* v);
int InitTrace(std::ofstream* TraceOutFile, std::string pid);

#endif // DEBUGGING_TRACER_H
