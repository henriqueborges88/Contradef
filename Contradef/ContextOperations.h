#pragma once

#ifndef CONTEXT_OPERATIONS_H
#define CONTEXT_OPERATIONS_H

#include "pin.H"
#include <iostream>
#include <string>
#include "regvalue_utils.h"
#include "Utils.h"
#include "InstructionSequenceDetector.h"
//#include "xed-interface.h"

VOID PrintRegisters(CONTEXT* ctxt);
VOID SetZfToOne(CONTEXT* ctxt);
VOID SetZfToZero(CONTEXT* ctxt);
VOID JumpToAddress(CONTEXT* ctxt, ADDRINT destAddr);
ADDRINT NextIpFrom(ADDRINT ip);

#endif // CONTEXT_OPERATIONS_H