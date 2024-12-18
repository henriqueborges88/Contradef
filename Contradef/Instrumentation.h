#pragma once

#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include "pin.H"
#include "params.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <deque>
#include <cwctype> // Para funções de verificação de caracteres wide, como iswalnum
#include "regvalue_utils.h"
#include "Utils.h"
#include "InstructionSequenceDetector.h"
#include "CallStackManager.h"
#include "DllFunctionTracker.h"

extern KNOB<bool> KnobDetailLevel; // Assume que KnobDetailLevel está definido em params.h

BOOL IsMainExecutable(ADDRINT address);
ADDRINT GetRtnAddr(ADDRINT instAddress);
std::string getFileName(const std::string& filePath);
VOID GetSectionInfo(IMG img, std::ofstream& OutFile);
VOID EmitFuncCall(THREADID threadid, ADDRINT instAddress, ADDRINT targetRtn, BOOL isCall, BOOL isDirect, INT32 tailCall, ADDRINT returnAddress, ADDRINT rspValue, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6);
VOID EmitFuncReturn(THREADID threadid, ADDRINT instAddress, ADDRINT retAddr, ADDRINT ret0, ADDRINT arg0, ADDRINT arg1, ADDRINT arg2, ADDRINT arg3, ADDRINT arg4, ADDRINT arg5, ADDRINT arg6);
VOID InstrumentFuncCall(TRACE trace, INS ins);
VOID TraceFunc(TRACE trace, VOID* v);
void saveAllExternalCalls(std::ofstream& ExternalCallTraceOutFile);
int InitInstrumentation();

#endif // INSTRUMENTATION_H