#include "InstGetMessageA.h"
#include "ThemidaDef.h"
#include <algorithm>
#include "SplashTarget.h"

std::map<CallContextKey, CallContext*> InstGetMessageA::callContextMap;
UINT32 InstGetMessageA::imgCallId = 0;
UINT32 InstGetMessageA::fcnCallId = 0;
Notifier* InstGetMessageA::globalNotifierPtr = nullptr;

// --- Resolução dinâmica de USER32 ---
// Evita depender de user32.lib e permite rodar em ambientes sem GUI.
typedef BOOL(WINAPI* PFN_PostMessageA) (WindowsAPI::HWND, UINT, WindowsAPI::WPARAM, WindowsAPI::LPARAM);
typedef BOOL(WINAPI* PFN_DestroyWindow)(WindowsAPI::HWND);
typedef BOOL(WINAPI* PFN_GetClientRect)(WindowsAPI::HWND, WindowsAPI::LPRECT);
typedef BOOL(WINAPI* PFN_ClientToScreen)(WindowsAPI::HWND, WindowsAPI::LPPOINT);

static PFN_PostMessageA    g_pPostMessageA = nullptr;
static PFN_DestroyWindow   g_pDestroyWindow = nullptr;
static PFN_GetClientRect   g_pGetClientRect = nullptr;
static PFN_ClientToScreen  g_pClientToScreen = nullptr;
static bool                g_user32Tried = false;

static void EnsureUser32Resolved()
{
    if (g_user32Tried) return;
    g_user32Tried = true;

    WindowsAPI::HMODULE hUser = WindowsAPI::GetModuleHandleA("user32.dll");
    if (!hUser) {
        // Em servidores/core ou sessão 0, isto pode falhar — trate retorno FALSE nos shims.
        hUser = WindowsAPI::LoadLibraryA("user32.dll");
    }
    if (hUser) {
        g_pPostMessageA = reinterpret_cast<PFN_PostMessageA>   (GetProcAddress(hUser, "PostMessageA"));
        g_pDestroyWindow = reinterpret_cast<PFN_DestroyWindow>  (GetProcAddress(hUser, "DestroyWindow"));
        g_pGetClientRect = reinterpret_cast<PFN_GetClientRect>  (GetProcAddress(hUser, "GetClientRect"));
        g_pClientToScreen = reinterpret_cast<PFN_ClientToScreen> (GetProcAddress(hUser, "ClientToScreen"));
    }
}

// ---------- helpers de acesso seguro ----------
static inline BOOL SafeRead(void* dst, const void* src, size_t sz) {
    return PIN_SafeCopy(dst, const_cast<void*>(src), sz) == sz;
}
static inline BOOL SafeWrite(void* dst, const void* src, size_t sz) {
    return PIN_SafeCopy(dst, const_cast<void*>(src), sz) == sz;
}

// Versões “dyn” que só chamam se o símbolo foi resolvido.
// Retornam FALSE se USER32 não estiver disponível.
inline BOOL GetClientRectDyn(WindowsAPI::HWND hWnd, WindowsAPI::LPRECT rc) {
    EnsureUser32Resolved();
    return g_pGetClientRect ? g_pGetClientRect(hWnd, rc) : FALSE;
}

inline BOOL ClientToScreenDyn(WindowsAPI::HWND hWnd, WindowsAPI::LPPOINT pt) {
    EnsureUser32Resolved();
    return g_pClientToScreen ? g_pClientToScreen(hWnd, pt) : FALSE;
}

// Helper para empacotar (x,y) em LPARAM sem usar MAKELPARAM (evita conflito com LONG)
inline WindowsAPI::LPARAM MakeMouseLPARAM(LONG x, LONG y) {
    return static_cast<WindowsAPI::LPARAM>(
        (static_cast<DWORD>(static_cast<WORD>(y)) << 16) |
        (static_cast<DWORD>(static_cast<WORD>(x)) & 0xFFFF)
        );
}

// ---------- leitura resumida de MSG (para log) ----------
static bool ReadMsgSummary(ADDRINT pMSG,
    ADDRINT& hwnd,
    UINT& message,
    ADDRINT& wParam,
    ADDRINT& lParam,
    DWORD& time,
    LONG& ptX,
    LONG& ptY)
{
    if (!pMSG) return false;

    const size_t P = sizeof(ADDRINT);

    size_t off_hwnd = 0;
    size_t off_msg = P;
    size_t off_wparam = P + 4 + ((P == 8) ? 4 : 0);
    size_t off_lparam = off_wparam + P;
    size_t off_time = off_lparam + P;
    size_t off_ptx = off_time + 4 + ((P == 8) ? 4 : 0);
    size_t off_pty = off_ptx + 4;

    WindowsAPI::SIZE_T c = 0;

    hwnd = 0; message = 0; wParam = 0; lParam = 0; time = 0; ptX = 0; ptY = 0;

    c = PIN_SafeCopy(&hwnd, reinterpret_cast<void*>(pMSG + off_hwnd), P); if (c != P) return false;
    c = PIN_SafeCopy(&message, reinterpret_cast<void*>(pMSG + off_msg), sizeof(message)); if (c != sizeof(message)) return false;
    c = PIN_SafeCopy(&wParam, reinterpret_cast<void*>(pMSG + off_wparam), P); if (c != P) return false;
    c = PIN_SafeCopy(&lParam, reinterpret_cast<void*>(pMSG + off_lparam), P); if (c != P) return false;
    c = PIN_SafeCopy(&time, reinterpret_cast<void*>(pMSG + off_time), sizeof(time)); if (c != sizeof(time)) return false;
    c = PIN_SafeCopy(&ptX, reinterpret_cast<void*>(pMSG + off_ptx), sizeof(ptX)); if (c != sizeof(ptX)) return false;
    c = PIN_SafeCopy(&ptY, reinterpret_cast<void*>(pMSG + off_pty), sizeof(ptY)); if (c != sizeof(ptY)) return false;

    return true;
}

// ---------- util: centro do cliente ----------
static bool ComputeCenterClient(WindowsAPI::HWND hWnd,
    WindowsAPI::POINT& ptClientCenter,
    WindowsAPI::POINT& ptScreenCenter)
{
    WindowsAPI::RECT rc{};
    if (!GetClientRectDyn(hWnd, &rc)) {
        return false; // sem USER32 ou hwnd inválido
    }

    WindowsAPI::LONG w = rc.right - rc.left;
    WindowsAPI::LONG h = rc.bottom - rc.top;
    WindowsAPI::LONG cx = rc.left + (w > 0 ? w / 2 : 0);
    WindowsAPI::LONG cy = rc.top + (h > 0 ? h / 2 : 0);

    ptClientCenter.x = cx;
    ptClientCenter.y = cy;

    ptScreenCenter = ptClientCenter;
    if (!ClientToScreenDyn(hWnd, &ptScreenCenter)) {
        return false; // sem USER32 (ex.: ambiente headless)
    }
    return true;
}

// ---------- BEFORE ----------
VOID InstGetMessageA::CallbackBefore(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT returnAddress,
    ADDRINT lpMsg, ADDRINT hWnd, ADDRINT wMsgFilterMin, ADDRINT wMsgFilterMax)
{
    if (instrumentOnlyMain && !IsMainExecutable(returnAddress)) {
        return;
    }

    ADDRINT rtnAddress = GetRtnAddr(instAddress);

    auto* args = new GetMessageAArgs;
    args->lpMsg = lpMsg;
    args->hWnd = hWnd;
    args->wMsgFilterMin = wMsgFilterMin;
    args->wMsgFilterMax = wMsgFilterMax;

    UINT32 callCtxId = callId * 100 + fcnCallId;

    auto* callContext = new CallContext(callCtxId, tid, rtnAddress, args);
    CallContextKey key = { callCtxId, tid };
    callContextMap[key] = callContext;
}

// ---------- AFTER ----------
VOID InstGetMessageA::CallbackAfter(THREADID tid, UINT32 callId, ADDRINT instAddress, ADDRINT /*rtn*/, CONTEXT* /*ctx*/,
    ADDRINT retValue,
    ADDRINT* returnAddress,
    ADDRINT lpMsg, ADDRINT hWnd, ADDRINT wMsgFilterMin, ADDRINT wMsgFilterMax)
{
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
        const GetMessageAArgs* args = reinterpret_cast<GetMessageAArgs*>(callContext->functionArgs);
        std::stringstream& ss = callContext->stringStream;

        // -------- log da GetMessageA --------
        ADDRINT msgHwnd = 0, msgWParam = 0, msgLParam = 0;
        UINT    msgId = 0;
        DWORD   msgTime = 0;
        LONG    ptX = 0, ptY = 0;
        bool haveMsg = ReadMsgSummary(args->lpMsg, msgHwnd, msgId, msgWParam, msgLParam, msgTime, ptX, ptY);

        ss << std::endl << "[+] " << RTN_Name(rtnCurrent) << "..." << std::endl;
        ss << "    Module name: " << IMG_Name(img) << std::endl;
        ss << "    Thread: " << tid << std::endl;
        ss << "    Call Id: " << fcnCallId << std::endl;
        ss << "    Routine address: " << std::hex << callContext->rtnAddress << std::dec << std::endl;

        ss << "    Parameters: " << std::endl;
        ss << "        lpMsg: 0x" << std::hex << (ADDRINT)args->lpMsg << std::dec << std::endl;
        ss << "        hWnd: 0x" << std::hex << (ADDRINT)args->hWnd << std::dec << std::endl;
        ss << "        wMsgFilterMin: " << (UINT)args->wMsgFilterMin << std::endl;
        ss << "        wMsgFilterMax: " << (UINT)args->wMsgFilterMax << std::endl;

        ss << "    Return Value (BOOL): " << (retValue ? "TRUE" : "FALSE") << std::endl;

        if (haveMsg) {
            ss << "    MSG filled:" << std::endl;
            ss << "        hwnd: 0x" << std::hex << msgHwnd << std::dec << std::endl;
            ss << "        message: 0x" << std::hex << msgId << std::dec << " (" << (UINT)msgId << ")" << std::endl;
            ss << "        wParam: 0x" << std::hex << msgWParam << std::dec << std::endl;
            ss << "        lParam: 0x" << std::hex << msgLParam << std::dec << std::endl;
            ss << "        time: " << msgTime << std::endl;
            ss << "        pt: (" << ptX << ", " << ptY << ")" << std::endl;
        }
        else {
            ss << "    (não foi possível ler o conteúdo de MSG)" << std::endl;
        }

        // --------- CONTRAMEDIDA: reescrever MSG para clique ---------
        // Somente se GetMessage retornou > 0 (mensagem válida)
        if ((INT64)retValue > 0) {
            WindowsAPI::HWND target = (WindowsAPI::HWND)g_SplashTarget;
            if (target) {
                LONG state = g_ClickState;  // 0,1,2,3
                if (state < 3) {
                    WindowsAPI::POINT ptC{}, ptS{};
                    if (!ComputeCenterClient(target, ptC, ptS)) {
                        // fallback mínimo para ambientes sem USER32
                        ptC.x = ptC.y = 1;
                        ptS = ptC;
                        ClientToScreenDyn(target, &ptS); // pode falhar silenciosamente
                        ClientToScreenDyn(target, &ptS); // pode falhar silenciosamente
                        ClientToScreenDyn(target, &ptS); // pode falhar silenciosamente
                    }

                    WindowsAPI::MSG fake{};
                    fake.hwnd = target;
                    fake.message = (state == 0) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
                    fake.wParam = (state == 0) ? MK_LBUTTON : 0;
                    fake.lParam = MakeMouseLPARAM(ptC.x, ptC.y);
                    fake.time = WindowsAPI::GetTickCount();
                    fake.pt = ptS;

                    if (SafeWrite(reinterpret_cast<VOID*>(args->lpMsg), &fake, sizeof(fake))) {
                        WindowsAPI::LONG next = (state == 0) ? 1 : (state == 1 ? 2 : 3);

                        // Garanta que a assinatura casa com seu wrapper (volatile LONG*)
                        WindowsAPI::InterlockedExchange(reinterpret_cast<WindowsAPI::LONG volatile*>(&g_ClickState),
                            next);

                        ss << "    [Countermeasure] Injected "
                            << ((fake.message == WM_LBUTTONDOWN) ? "WM_LBUTTONDOWN" : "WM_LBUTTONUP")
                            << " at (" << ptC.x << "," << ptC.y << ") hwnd=0x"
                            << std::hex << (ADDRINT)target << std::dec
                            << " state=" << next << std::endl;

                        if (next == 3) {
                            g_SplashTarget = NULL;
                            ss << "    [Countermeasure] Done — target cleared." << std::endl;
                        }
                    }
                    else {
                        ss << "    [Countermeasure] SafeWrite(MSG) failed; skipping." << std::endl;
                    }
                }

            }
        }
        // --------- fim contramedida ---------

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

// ---------- registro ----------
VOID InstGetMessageA::InstrumentFunction(RTN rtn, Notifier& globalNotifier)
{
    std::string rtnName = RTN_Name(rtn);
    if (rtnName == "GetMessageA") {
        imgCallId++;
        globalNotifierPtr = &globalNotifier;

        RTN_Open(rtn);

        // BOOL GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);

        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(CallbackBefore),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_RETURN_IP,                   // endereço do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpMsg
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // wMsgFilterMin
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // wMsgFilterMax
            IARG_END);

        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(CallbackAfter),
            IARG_THREAD_ID,
            IARG_UINT32, imgCallId,
            IARG_INST_PTR,
            IARG_ADDRINT, RTN_Address(rtn),
            IARG_CONTEXT,
            IARG_FUNCRET_EXITPOINT_VALUE,     // BOOL retorno
            IARG_REG_VALUE, REG_STACK_PTR,    // endereço de retorno do chamador
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0, // lpMsg
            IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // hWnd
            IARG_FUNCARG_ENTRYPOINT_VALUE, 2, // wMsgFilterMin
            IARG_FUNCARG_ENTRYPOINT_VALUE, 3, // wMsgFilterMax
            IARG_END);

        RTN_Close(rtn);
    }
}
