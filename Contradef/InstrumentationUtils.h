#pragma once
#ifndef INSTRUMENTATION_UTILS_H
#define INSTRUMENTATION_UTILS_H

#include "pin.H"
#include <iostream>

BOOL IsMainExecutable(ADDRINT address);
ADDRINT GetRtnAddr(ADDRINT instAddress);
VOID PauseAtAddress(ADDRINT address);

#endif // INSTRUMENTATION_UTILS_H