#include "InstCreateWindowExA.h"
#include "ThemidaDef.h"
#include "SplashTarget.h"

std::map<CallContextKey, CallContext*> InstCreateWindowExA::callContextMap;
UINT32 InstCreateWindowExA::imgCallId = 0;
UINT32 InstCreateWindowExA::fcnCallId = 0;
Notifier* InstCreateWindowExA::globalNotifierPtr = nullptr;

// --- USER32/KERNEL32 dinamicamente ---
// --- USER32 / KERNEL32 dinamicamente (aditivos) ---
typedef BOOL(WINAPI* PFN_ShowWindow)(WindowsAPI::HWND, int);
typedef BOOL(WINAPI* PFN_SetCursorPos)(int, int);
typedef BOOL(WINAPI* PFN_GetWindowRect)(WindowsAPI::HWND, WindowsAPI::LPRECT);
typedef UINT(WINAPI* PFN_GetSystemMetrics)(int);
typedef DWORD(WINAPI* PFN_GetWindowThreadProcessId)(WindowsAPI::HWND, WindowsAPI::LPDWORD);
typedef BOOL(WINAPI* PFN_AttachThreadInput)(DWORD, DWORD, BOOL);

static PFN_ShowWindow                g_ShowWindow = nullptr;
static PFN_SetCursorPos              g_SetCursorPos = nullptr;
static PFN_GetSystemMetrics          g_GetSystemMetrics = nullptr;
static PFN_GetWindowThreadProcessId  g_GetWindowThreadProcessId = nullptr;
static PFN_AttachThreadInput         g_AttachThreadInput = nullptr;

typedef BOOL(WINAPI* PFN_PostMessageA)(WindowsAPI::HWND, UINT, WindowsAPI::WPARAM, WindowsAPI::LPARAM);
typedef WindowsAPI::LRESULT(WINAPI* PFN_SendMessageA)(WindowsAPI::HWND, UINT, WindowsAPI::WPARAM, WindowsAPI::LPARAM);
typedef WindowsAPI::LRESULT(WINAPI* PFN_SendMessageTimeoutA)(WindowsAPI::HWND, UINT, WindowsAPI::WPARAM, WindowsAPI::LPARAM, UINT, UINT, WindowsAPI::PDWORD_PTR);
typedef WindowsAPI::HWND(WINAPI* PFN_FindWindowExA)(WindowsAPI::HWND, WindowsAPI::HWND, WindowsAPI::LPCSTR, WindowsAPI::LPCSTR);
typedef int    (WINAPI* PFN_GetClassNameA)(WindowsAPI::HWND, WindowsAPI::LPSTR, int);
typedef int    (WINAPI* PFN_GetWindowTextA)(WindowsAPI::HWND, WindowsAPI::LPSTR, int);
typedef WindowsAPI::LONG_PTR(WINAPI* PFN_GetWindowLongPtrA)(WindowsAPI::HWND, int);
typedef int    (WINAPI* PFN_GetDlgCtrlID)(WindowsAPI::HWND);
typedef BOOL(WINAPI* PFN_GetWindowRect)(WindowsAPI::HWND, WindowsAPI::LPRECT);
typedef BOOL(WINAPI* PFN_ScreenToClient)(WindowsAPI::HWND, WindowsAPI::LPPOINT);
typedef BOOL(WINAPI* PFN_SetForegroundWindow)(WindowsAPI::HWND);
typedef UINT(WINAPI* PFN_MapVirtualKeyA)(UINT, UINT);
typedef UINT(WINAPI* PFN_SendInput)(UINT, WindowsAPI::LPINPUT, int);

// Kernel32 (thread/tempo)
typedef WindowsAPI::HANDLE(WINAPI* PFN_CreateThread)(WindowsAPI::LPSECURITY_ATTRIBUTES, WindowsAPI::SIZE_T, WindowsAPI::LPTHREAD_START_ROUTINE, WindowsAPI::LPVOID, WindowsAPI::DWORD, WindowsAPI::LPDWORD);
typedef VOID(WINAPI* PFN_Sleep)(DWORD);

// Ponteiros globais
static PFN_PostMessageA         g_PostMessageA = nullptr;
static PFN_SendMessageA         g_SendMessageA = nullptr;
static PFN_SendMessageTimeoutA  g_SendMessageTimeoutA = nullptr;
static PFN_FindWindowExA        g_FindWindowExA = nullptr;
static PFN_GetClassNameA        g_GetClassNameA = nullptr;
static PFN_GetWindowTextA       g_GetWindowTextA = nullptr;
static PFN_GetWindowLongPtrA    g_GetWindowLongPtrA = nullptr;
static PFN_GetDlgCtrlID         g_GetDlgCtrlID = nullptr;
static PFN_GetWindowRect        g_GetWindowRect = nullptr;
static PFN_ScreenToClient       g_ScreenToClient = nullptr;
static PFN_SetForegroundWindow  g_SetForegroundWindow = nullptr;
static PFN_SendInput            g_SendInput = nullptr;
static PFN_MapVirtualKeyA       g_MapVirtualKeyA = nullptr;

static PFN_CreateThread         g_CreateThread = nullptr;
static PFN_Sleep                g_Sleep = nullptr;

static bool g_user32Resolved = false;

static void EnsureUser32Resolved()
{
    if (g_user32Resolved) return;
    g_user32Resolved = true;

    WindowsAPI::HMODULE hUser = WindowsAPI::GetModuleHandleA("user32.dll");
    if (!hUser) hUser = WindowsAPI::LoadLibraryA("user32.dll");

    if (hUser) {
        g_PostMessageA = (PFN_PostMessageA)GetProcAddress(hUser, "PostMessageA");
        g_SendMessageA = (PFN_SendMessageA)GetProcAddress(hUser, "SendMessageA");
        g_SendMessageTimeoutA = (PFN_SendMessageTimeoutA)GetProcAddress(hUser, "SendMessageTimeoutA");
        g_FindWindowExA = (PFN_FindWindowExA)GetProcAddress(hUser, "FindWindowExA");
        g_GetClassNameA = (PFN_GetClassNameA)GetProcAddress(hUser, "GetClassNameA");
        g_GetWindowTextA = (PFN_GetWindowTextA)GetProcAddress(hUser, "GetWindowTextA");
        g_GetWindowLongPtrA = (PFN_GetWindowLongPtrA)GetProcAddress(hUser, "GetWindowLongPtrA");
        g_GetDlgCtrlID = (PFN_GetDlgCtrlID)GetProcAddress(hUser, "GetDlgCtrlID");
        g_GetWindowRect = (PFN_GetWindowRect)GetProcAddress(hUser, "GetWindowRect");
        g_ScreenToClient = (PFN_ScreenToClient)GetProcAddress(hUser, "ScreenToClient");
        g_SetForegroundWindow = (PFN_SetForegroundWindow)GetProcAddress(hUser, "SetForegroundWindow");
        g_SendInput = (PFN_SendInput)GetProcAddress(hUser, "SendInput");
        g_MapVirtualKeyA = (PFN_MapVirtualKeyA)GetProcAddress(hUser, "MapVirtualKeyA");
        g_ShowWindow = (PFN_ShowWindow)GetProcAddress(hUser, "ShowWindow");
        g_SetCursorPos = (PFN_SetCursorPos)GetProcAddress(hUser, "SetCursorPos");
        g_GetSystemMetrics = (PFN_GetSystemMetrics)GetProcAddress(hUser, "GetSystemMetrics");
        g_GetWindowThreadProcessId = (PFN_GetWindowThreadProcessId)GetProcAddress(hUser, "GetWindowThreadProcessId");
        g_AttachThreadInput = (PFN_AttachThreadInput)GetProcAddress(hUser, "AttachThreadInput");
    }

    WindowsAPI::HMODULE hK32 = WindowsAPI::GetModuleHandleA("kernel32.dll");
    if (hK32) {
        g_CreateThread = (PFN_CreateThread)GetProcAddress(hK32, "CreateThread");
        g_Sleep = (PFN_Sleep)GetProcAddress(hK32, "Sleep");
    }
}

static bool IsButtonClass(WindowsAPI::HWND h)
{
    if (!g_GetClassNameA) return false;
    char cls[32] = {};
    if (g_GetClassNameA(h, cls, (int)sizeof(cls)) <= 0) return false;
    for (char& c : cls) c = (char)tolower((unsigned char)c);
    return strcmp(cls, "button") == 0;
}

static bool TextLooksDismiss(const std::string& t)
{
    // Alvos comuns
    static const char* keys[] = { "ok", "yes", "sim", "continuar", "continue", "next",
                                  "aceitar", "accept", "trial", "avaliar", "fechar", "close",
                                  "entendi", "got it" };
    std::string s = t; std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    for (auto k : keys) { if (s.find(k) != std::string::npos) return true; }
    return false;
}

static WindowsAPI::HWND FindDismissButton(WindowsAPI::HWND parent)
{
    if (!g_FindWindowExA || !g_GetWindowTextA) return nullptr;

    WindowsAPI::HWND child = nullptr;
    char buf[128];

    for (;;) {
        child = g_FindWindowExA(parent, child, nullptr, nullptr);
        if (!child) break;

        if (IsButtonClass(child)) {
            buf[0] = 0;
            g_GetWindowTextA(child, buf, (int)sizeof(buf));
            if (TextLooksDismiss(buf)) return child;
        }
        // percorre recursivamente a árvore (profundidade pequena)
        WindowsAPI::HWND gchild = FindDismissButton(child);
        if (gchild) return gchild;
    }
    return nullptr;
}

static bool PostBMClick(WindowsAPI::HWND hBtn)
{
    if (!g_PostMessageA) return false;
    // BM_CLICK = 0x00F5
    return !!g_PostMessageA(hBtn, 0x00F5, 0, 0);
}

static bool ClickCenterViaSendInput(WindowsAPI::HWND hTarget)
{
    EnsureUser32Resolved();
    if (!g_SendInput || !g_GetWindowRect || !g_GetSystemMetrics) return false;

    WindowsAPI::RECT r{};
    if (!g_GetWindowRect(hTarget, &r)) return false;

    // Coordenadas do virtual desktop (multi-monitor)
    const int vx = (int)g_GetSystemMetrics(76 /*SM_XVIRTUALSCREEN*/);
    const int vy = (int)g_GetSystemMetrics(77 /*SM_YVIRTUALSCREEN*/);
    const int vcx = (int)g_GetSystemMetrics(78 /*SM_CXVIRTUALSCREEN*/);
    const int vcy = (int)g_GetSystemMetrics(79 /*SM_CYVIRTUALSCREEN*/);

    // Centro da janela em coordenadas de tela
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;

    // Normaliza para 0..65535 (SendInput + ABSOLUTE + VIRTUALDESK)
    auto norm = [](int p, int origin, int extent) -> LONG {
        if (extent <= 1) return 0;
        // (p - origin) / (extent-1) * 65535
        const double rel = double(p - origin) / double(extent - 1);
        long v = (long)(rel * 65535.0);
        if (v < 0) v = 0; else if (v > 65535) v = 65535;
        return (LONG)v;
        };
    const LONG nx = norm(cx, vx, vcx);
    const LONG ny = norm(cy, vy, vcy);

    // Traz para foreground com attach de input (evita rejeição de foco)
    if (g_SetForegroundWindow && g_ShowWindow && g_GetWindowThreadProcessId && g_AttachThreadInput) {
        DWORD tgtTid = 0;
        g_GetWindowThreadProcessId(hTarget, &tgtTid);
        DWORD curTid = (DWORD)::WindowsAPI::GetCurrentThreadId();

        if (tgtTid && curTid && g_AttachThreadInput(curTid, tgtTid, TRUE)) {
            g_ShowWindow(hTarget, 5 /*SW_SHOW*/);
            g_SetForegroundWindow(hTarget);
            // pequeno atraso
            if (g_Sleep) g_Sleep(30);
            g_AttachThreadInput(curTid, tgtTid, FALSE);
        }
        else {
            g_ShowWindow(hTarget, 5);
            g_SetForegroundWindow(hTarget);
            if (g_Sleep) g_Sleep(30);
        }
    }

    WindowsAPI::INPUT in[4] = {};
    // move
    in[0].type = INPUT_MOUSE;
    in[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in[0].mi.dx = nx;
    in[0].mi.dy = ny;

    // pequeno “move” extra para parecer humano
    in[1] = in[0];
    in[1].mi.dx = nx + 1;

    // down
    in[2].type = INPUT_MOUSE;
    in[2].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in[2].mi.dx = nx; in[2].mi.dy = ny;

    // up
    in[3].type = INPUT_MOUSE;
    in[3].mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    in[3].mi.dx = nx; in[3].mi.dy = ny;

    UINT sent = g_SendInput(2, &in[0], sizeof(WindowsAPI::INPUT));
    if (g_Sleep) g_Sleep(35);
    sent += g_SendInput(2, &in[2], sizeof(WindowsAPI::INPUT));

    if (sent < 4) {
        // fallback: posiciona cursor via SetCursorPos + down/up relativo
        if (g_SetCursorPos) {
            g_SetCursorPos(cx, cy);
            if (g_Sleep) g_Sleep(20);
            WindowsAPI::INPUT f[2] = {};
            f[0].type = INPUT_MOUSE; f[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            f[1].type = INPUT_MOUSE; f[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            return g_SendInput(2, f, sizeof(WindowsAPI::INPUT)) == 2;
        }
        return false;
    }
    return true;
}

static bool ClickViaMessages(WindowsAPI::HWND hTarget)
{
    EnsureUser32Resolved();
    if (!g_GetWindowRect || !g_SendMessageTimeoutA) return false;

    WindowsAPI::RECT r{};
    if (!g_GetWindowRect(hTarget, &r)) return false;

    // centro em cliente
    int cx = (r.right - r.left) / 2;
    int cy = (r.bottom - r.top) / 2;
    WindowsAPI::LPARAM lp = (WindowsAPI::LPARAM)((cy << 16) | (cx & 0xFFFF));

    WindowsAPI::DWORD_PTR out = 0;
    auto SMT = [&](UINT msg, WindowsAPI::WPARAM wp) -> BOOL {
        return g_SendMessageTimeoutA(hTarget, msg, wp, lp,
            0x0002 /*SMTO_ABORTIFHUNG*/, 100, &out);
        };

    SMT(0x0200 /*WM_MOUSEMOVE*/, 0);                 // move
    SMT(0x0201 /*WM_LBUTTONDOWN*/, 1 /*MK_LBUTTON*/);
    if (g_Sleep) g_Sleep(30);
    return !!SMT(0x0202 /*WM_LBUTTONUP*/, 0);
}



struct ClickWork {
    WindowsAPI::HWND hWnd;
    std::string cls;
    std::string title;
};

static DWORD WINAPI ClickWorkerProc(LPVOID p)
{
    EnsureUser32Resolved();

    ClickWork* w = static_cast<ClickWork*>(p);
    if (!w) return 0;
    WindowsAPI::HWND h = w->hWnd;
    delete w;                    // libera aqui — não usamos unique_ptr

    if (g_Sleep) g_Sleep(120);   // pequeno delay para a splash terminar de pintar

    // 1) tentativa via SendInput (absoluto / virtualdesk)
    if (ClickCenterViaSendInput(h)) return 1;

    // 2) fallback por mensagens (WM_MOUSEMOVE/DOWN/UP)
    if (ClickViaMessages(h)) return 1;

    return 0;
}




// --- helpers locais ---
static inline bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

static inline bool str_contains_ci(std::string s, const char* needle) {
    if (!needle) return false;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    std::string n = needle; std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return s.find(n) != std::string::npos;
}

static bool IsSplashLike(DWORD exStyle, DWORD style, const std::string& cls, const std::string& title) {
    const bool popupVisible = (style & (WS_POPUP | WS_VISIBLE)) == (WS_POPUP | WS_VISIBLE);
    if (!popupVisible) return false;

    const bool topMost = (exStyle & WS_EX_TOPMOST) != 0;
    const bool clsSplash = iequals(cls, "SplashClassName") || cls == "#32770"; // dialog box comum
    const bool titleSplash = str_contains_ci(title, "splash")
        || str_contains_ci(title, "warning")
        || str_contains_ci(title, "trial");

    return topMost || clsSplash || titleSplash;
}

VOID InstCreateWindowExA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT returnAddress,
    ADDRINT dwExStyle, ADDRINT lpClassName, ADDRINT lpWindowName, ADDRINT dwStyle,
    ADDRINT X, ADDRINT Y, ADDRINT nWidth, ADDRINT nHeight,
    ADDRINT hWndParent, ADDRINT hMenu, ADDRINT hInstance, ADDRINT lpParam) {

    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    auto* args = new CreateWindowExAArgs;
    args->dwExStyle = dwExStyle;
    args->lpClassName = lpClassName;
    args->lpWindowName = lpWindowName;
    args->dwStyle = dwStyle;
    args->X = X;
    args->Y = Y;
    args->nWidth = nWidth;
    args->nHeight = nHeight;
    args->hWndParent = hWndParent;
    args->hMenu = hMenu;
    args->hInstance = hInstance;
    args->lpParam = lpParam;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;
}

VOID InstCreateWindowExA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT retValue,
    ADDRINT* returnAddress,
    ADDRINT dwExStyle, ADDRINT lpClassName, ADDRINT lpWindowName, ADDRINT dwStyle,
    ADDRINT X, ADDRINT Y, ADDRINT nWidth, ADDRINT nHeight,
    ADDRINT hWndParent, ADDRINT hMenu, ADDRINT hInstance, ADDRINT lpParam) {

    if (instrumentOnlyMain && !IsMainExecutable(*returnAddress)) {
        return;
    }

    UINT32 callCtxId = callId * 100 + fcnCallId;
    CallContextKey key = { callCtxId, tid };
    auto it = callContextMap.find(key);

    if (it != callContextMap.end()) {
        PIN_LockClient();
        IMG img = IMG_FindByAddress(instAddress);
        RTN rtnCurrent = RTN_FindByAddress(instAddress);
        PIN_UnlockClient();

        CallContext* callContext = it->second;
        const CreateWindowExAArgs* args = reinterpret_cast<CreateWindowExAArgs*>(callContext->functionArgs);
        std::stringstream& ss = callContext->stringStream;

        std::string cls = ConvertAddrToAnsiString(args->lpClassName);
        std::string text = ConvertAddrToAnsiString(args->lpWindowName);

        ss << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        ss << "    Module name: " << IMG_Name(img) << std::endl;
        ss << "    Thread: " << tid << std::endl;
        ss << "    Call Id: " << fcnCallId << std::endl;
        ss << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;

        ss << "    Parameters: " << std::endl;
        ss << "        dwExStyle: 0x" << std::hex << (DWORD)args->dwExStyle << std::dec << std::endl;
        ss << "        lpClassName: " << cls << std::endl;
        ss << "        lpWindowName: " << text << std::endl;
        ss << "        dwStyle: 0x" << std::hex << (DWORD)args->dwStyle << std::dec << std::endl;
        ss << "        X: " << (INT32)args->X << std::endl;
        ss << "        Y: " << (INT32)args->Y << std::endl;
        ss << "        nWidth: " << (INT32)args->nWidth << std::endl;
        ss << "        nHeight: " << (INT32)args->nHeight << std::endl;
        ss << "        hWndParent: 0x" << std::hex << (ADDRINT)args->hWndParent << std::dec << std::endl;
        ss << "        hMenu: 0x" << std::hex << (ADDRINT)args->hMenu << std::dec << std::endl;
        ss << "        hInstance: 0x" << std::hex << (ADDRINT)args->hInstance << std::dec << std::endl;
        ss << "        lpParam: 0x" << std::hex << (ADDRINT)args->lpParam << std::dec << std::endl;

        ss << "    Return Value (HWND): 0x" << std::hex << (ADDRINT)retValue << std::dec << std::endl;

        // --- CONTRAMEDIDA: fechar splash/dialog recém-criada ---
        {
            //using namespace WindowsAPI;
            WindowsAPI::HWND hWnd = reinterpret_cast<WindowsAPI::HWND>(retValue);
            DWORD exs = static_cast<DWORD>(args->dwExStyle);
            DWORD sty = static_cast<DWORD>(args->dwStyle);

            bool suspicious = (hWnd != nullptr) && IsSplashLike(exs, sty, cls, text);
            if (suspicious) {

                //Fallback para problema de click no splash do temida. Reativando a instrumentação de memória e instruções
                if (IsStringInStrategyScope("Themida")) {
                    if (KnobTraceInstructions) {
                        TraceInstructions::disable();
                    }
                    if (KnobTraceMemory) {
                        TraceMemory::disable();
                    }
                }
                // Fim fallback

                WindowsAPI::HWND hWnd = reinterpret_cast<WindowsAPI::HWND>(retValue);
                g_SplashTarget = hWnd;

                WindowsAPI::InterlockedExchange((WindowsAPI::LONG*)&g_ClickState, 0);

                ss << "    [Countermeasure] Splash alvo registrado para auto-click -> HWND=0x"
                    << std::hex << (ADDRINT)hWnd << std::dec
                    << ", class=\"" << cls << "\", title=\"" << text << "\"\n";




                //EnsureUser32Resolved();

                //// disparamos em thread separado pra mimetizar usuário e não travar o callback
                //if (g_CreateThread) {
                //    ClickWork* work = new ClickWork{ hWnd, cls, text };
                //    HANDLE h = g_CreateThread(nullptr, 0, ClickWorkerProc, work, 0, nullptr);
                //    if (!h) delete work;

                //    ss << "    [Countermeasure] Scheduled auto-click for HWND=0x"
                //        << std::hex << (ADDRINT)hWnd << std::dec
                //        << " class=\"" << cls << "\" title=\"" << text << "\"\n";
                //}
                //else {
                //    // fallback síncrono (último caso)
                //    WindowsAPI::HWND btn = FindDismissButton(hWnd);
                //    bool ok = btn ? PostBMClick(btn) : ClickCenterViaSendInput(hWnd);
                //    ss << "    [Countermeasure] Immediate "
                //        << (btn ? "BM_CLICK" : "center-click")
                //        << (ok ? " ok\n" : " failed\n");
                //}
            }
        }
        // FIM CONTRAMEDIDA

        ss << "[*] Completed" << std::endl << std::endl;


        ExecutionInformation info = { ss.str() };
        ExecutionEventData event(info);
        globalNotifierPtr->NotifyAll(&event);

        delete args;
        delete callContext;
        callContextMap.erase(it);
    }

    fcnCallId++;
}

VOID InstCreateWindowExA::InstrumentFunction(RTN rtn, Notifier& globalNotifier) {

    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "CreateWindowExA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // HWND CreateWindowExA(...)

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                           // endereço do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,         // dwExStyle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,         // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,         // lpWindowName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,         // dwStyle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,         // X
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,         // Y
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6,         // nWidth
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7,         // nHeight
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8,         // hWndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9,         // hMenu
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10,        // hInstance
            IARG_FUNCARG_ENTRYPOINT_VALUE, 11,        // lpParam
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,             // HWND retorno
            IARG_REG_VALUE, REG_STACK_PTR,            // endereço de retorno do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,         // dwExStyle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1,         // lpClassName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2,         // lpWindowName
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3,         // dwStyle
            IARG_FUNCARG_ENTRYPOINT_VALUE, 4,         // X
            IARG_FUNCARG_ENTRYPOINT_VALUE, 5,         // Y
            IARG_FUNCARG_ENTRYPOINT_VALUE, 6,         // nWidth
            IARG_FUNCARG_ENTRYPOINT_VALUE, 7,         // nHeight
            IARG_FUNCARG_ENTRYPOINT_VALUE, 8,         // hWndParent
            IARG_FUNCARG_ENTRYPOINT_VALUE, 9,         // hMenu
            IARG_FUNCARG_ENTRYPOINT_VALUE, 10,        // hInstance
            IARG_FUNCARG_ENTRYPOINT_VALUE, 11,        // lpParam
            IARG_END);

        RTN_Close(rtn);
    }
}
