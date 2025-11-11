#pragma once
#include "utils.h"

extern volatile WindowsAPI::HWND g_SplashTarget;   // janela candidata
extern volatile WindowsAPI::LONG             g_ClickState;     // 0=idle, 1=down injetado, 2=up injetado, 3=feito
