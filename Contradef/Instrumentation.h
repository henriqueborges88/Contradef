#pragma once

#include "pin.H"
#include "params.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <deque>

extern KNOB<bool> KnobDetailLevel; // Assume que KnobDetailLevel está definido em params.h

BOOL IsMainExecutable(ADDRINT address);
ADDRINT GetRtnAddr(ADDRINT instAddress);
VOID GetSectionInfo(IMG img, std::ofstream& OutFile);