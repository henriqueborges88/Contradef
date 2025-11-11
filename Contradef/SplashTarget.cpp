#include "SplashTarget.h"

volatile WindowsAPI::HWND g_SplashTarget = nullptr;
volatile WindowsAPI::LONG             g_ClickState = 0;
