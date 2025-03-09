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
#include "InstrumentationUtils.h"
#include "InstructionSequenceDetector.h"
#include "CallStackManager.h"
#include "DllFunctionTracker.h"
#include "DllFunctionMapper.h"
#include "TraceInstructions.h"
#include "TraceMemory.h"
#include "InstrumentationCallbacks.h"
#include "TraceFcnCall.h"
#include "TraceDisassembly.h"


extern KNOB<bool> KnobDetailLevel; // Assume que KnobDetailLevel está definido em params.h

BOOL IsMainExecutable(ADDRINT address);
ADDRINT GetRtnAddr(ADDRINT instAddress);
std::string getFileName(const std::string& filePath);
VOID GetSectionInfo(IMG img, std::ofstream& OutFile);
VOID PauseAtEntryPoint(ADDRINT entryAddress);
VOID PauseAtAddress(ADDRINT address);
VOID InstrumentFuncCall(TRACE trace, INS ins);
void saveAllExternalCalls(std::ofstream& ExternalCallTraceOutFile);
int InitInstrumentation(std::string pid);

#endif // INSTRUMENTATION_H