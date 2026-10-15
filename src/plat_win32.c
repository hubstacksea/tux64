/* plat_win32.c - Windows backend: Win32 + WGL, XInput (loaded at runtime), waveOut audio */
#define WIN32_LEAN_AND_MEAN
#include "platform.h"
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <string.h>
#include <stdio.h>

unsigned char plat_keys[K_COUNT];
Gamepad plat_pad;

static HINSTANCE hinst;
static HWND hwnd;
static HDC hdc;
static HGLRC hrc;
static int win_w = 640, win_h = 480;
static int quit_req;
static int is_fullscreen;
static RECT saved_rect;
static LONG saved_style;
static LARGE_INTEGER perf_freq;

/* ---- XInput (dynamically loaded) ---- */
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XPadState;
typedef struct { DWORD dwPacketNumber; XPadState Gamepad; } XState;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XState *);
static XInputGetState_t pXInputGetState;
static double xi_retry;

static int map_vk(WPARAM vk, LPARAM lp)
{
    switch (vk) {
    case VK_UP: return K_UP; case VK_DOWN: return K_DOWN;
    case VK_LEFT: return K_LEFT; case VK_RIGHT: return K_RIGHT;
    case 'W': return K_W; case 'A': return K_A; case 'S': return K_S; case 'D': return K_D;
    case VK_SPACE: return K_SPACE;
    case 'J': return K_J; case 'K': return K_K; case 'L': return K_L; case 'I': return K_I;
    case 'U': return K_U; case 'O': return K_O; case 'P': return K_P;
    case 'Z': return K_Z; case 'X': return K_X; case 'C': return K_C; case 'V': return K_V;
    case 'Q': return K_Q; case 'E': return K_E; case 'R': return K_R; case 'F': return K_F;
    case VK_SHIFT: return ((lp >> 16) & 0xFF) == 0x36 ? K_RSHIFT : K_LSHIFT;
    case VK_CONTROL: return (lp & (1 << 24)) ? K_RCTRL : K_LCTRL;
    case VK_RETURN: return K_ENTER; case VK_ESCAPE: return K_ESC; case VK_TAB: return K_TAB;
    case VK_F11: return K_F11; case VK_F12: return K_F12; case VK_BACK: return K_BACKSPACE;
    case VK_MENU: return K_ALT;
    case VK_PRIOR: return K_PGUP; case VK_NEXT: return K_PGDN;
    case 'M': return K_M;
    }
    return -1;
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE: quit_req = 1; return 0;
    case WM_SIZE: win_w = LOWORD(lp); win_h = HIWORD(lp); return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        int k = map_vk(wp, lp);
        if (k >= 0) plat_keys[k] = 1;
        if (msg == WM_SYSKEYDOWN && wp == VK_F4) quit_req = 1;
        return 0;
    }
    case WM_KEYUP: case WM_SYSKEYUP: {
        int k = map_vk(wp, lp);
        if (k >= 0) plat_keys[k] = 0;
        return 0;
    }
    case WM_SYSCHAR: return 0;  /* suppress beep on Alt+key */
    case WM_KILLFOCUS: memset(plat_keys, 0, sizeof plat_keys); return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

typedef BOOL (WINAPI *wglSwapIntervalEXT_t)(int);
typedef BOOL (WINAPI *SetProcessDPIAware_t)(void);

int plat_init(const char *title, int w, int h)
{
    WNDCLASSA wc;
    RECT r;
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    DWORD style = WS_OVERLAPPEDWINDOW;
    HMODULE user32 = GetModuleHandleA("user32.dll");

    if (user32) {
        SetProcessDPIAware_t f = (SetProcessDPIAware_t)(void *)GetProcAddress(user32, "SetProcessDPIAware");
        if (f) f();
    }
    QueryPerformanceFrequency(&perf_freq);
    timeBeginPeriod(1);
    hinst = GetModuleHandleA(NULL);
    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = "Tux64Window";
    if (!RegisterClassA(&wc)) return 0;
    r.left = 0; r.top = 0; r.right = w; r.bottom = h;
    AdjustWindowRect(&r, style, FALSE);
    hwnd = CreateWindowExA(0, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top, NULL, NULL, hinst, NULL);
    if (!hwnd) return 0;
    hdc = GetDC(hwnd);
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf || !SetPixelFormat(hdc, pf, &pfd)) return 0;
    hrc = wglCreateContext(hdc);
    if (!hrc || !wglMakeCurrent(hdc, hrc)) return 0;
    {
        wglSwapIntervalEXT_t si = (wglSwapIntervalEXT_t)(void *)wglGetProcAddress("wglSwapIntervalEXT");
        if (si) si(1);
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    GetClientRect(hwnd, &r);
    win_w = r.right; win_h = r.bottom;
    {
        static const char *dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
        int i;
        for (i = 0; i < 3 && !pXInputGetState; i++) {
            HMODULE m = LoadLibraryA(dlls[i]);
            if (m) pXInputGetState = (XInputGetState_t)(void *)GetProcAddress(m, "XInputGetState");
        }
    }
    return 1;
}

void plat_set_title(const char *t) { if (hwnd) SetWindowTextA(hwnd, t); }

int plat_system_lang_ja(void)
{
    return (GetUserDefaultUILanguage() & 0x3FF) == 0x11;   /* LANG_JAPANESE */
}

void plat_shutdown(void)
{
    plat_audio_shutdown();
    if (hrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(hrc); hrc = NULL; }
    if (hwnd) { ReleaseDC(hwnd, hdc); DestroyWindow(hwnd); hwnd = NULL; }
    timeEndPeriod(1);
}

static float stick(SHORT v, int dead)
{
    float f;
    if (v > -dead && v < dead) return 0.0f;
    f = v / 32767.0f;
    if (f < -1) f = -1;
    if (f > 1) f = 1;
    return f;
}

static void poll_xinput(void)
{
    XState st;
    double t = plat_time();
    if (!pXInputGetState) return;
    if (!plat_pad.connected && t < xi_retry) return;
    if (pXInputGetState(0, &st) != 0) {
        plat_pad.connected = 0;
        xi_retry = t + 1.5;
        return;
    }
    plat_pad.connected = 1;
    plat_pad.lx = stick(st.Gamepad.sThumbLX, 7849);
    plat_pad.ly = stick(st.Gamepad.sThumbLY, 7849);
    plat_pad.rx = stick(st.Gamepad.sThumbRX, 8689);
    plat_pad.ry = stick(st.Gamepad.sThumbRY, 8689);
    {
        WORD b = st.Gamepad.wButtons;
        unsigned o = 0;
        if (b & 0x1000) o |= GP_A;
        if (b & 0x2000) o |= GP_B;
        if (b & 0x4000) o |= GP_X;
        if (b & 0x8000) o |= GP_Y;
        if (b & 0x0100) o |= GP_LB;
        if (b & 0x0200) o |= GP_RB;
        if (b & 0x0020) o |= GP_BACK;
        if (b & 0x0010) o |= GP_START;
        if (b & 0x0001) o |= GP_DUP;
        if (b & 0x0002) o |= GP_DDOWN;
        if (b & 0x0004) o |= GP_DLEFT;
        if (b & 0x0008) o |= GP_DRIGHT;
        if (st.Gamepad.bLeftTrigger > 60) o |= GP_LT;
        if (st.Gamepad.bRightTrigger > 60) o |= GP_RT;
        plat_pad.btn = o;
    }
}

int plat_pump(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) quit_req = 1;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    poll_xinput();
    return !quit_req;
}

void plat_swap(void) { SwapBuffers(hdc); }
void plat_size(int *w, int *h) { *w = win_w; *h = win_h; }

double plat_time(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)perf_freq.QuadPart;
}

void plat_sleep_ms(int ms) { if (ms > 0) Sleep((DWORD)ms); }

void plat_toggle_fullscreen(void)
{
    if (!is_fullscreen) {
        MONITORINFO mi;
        mi.cbSize = sizeof mi;
        GetWindowRect(hwnd, &saved_rect);
        saved_style = GetWindowLongA(hwnd, GWL_STYLE);
        if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            SetWindowLongA(hwnd, GWL_STYLE, (saved_style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | WS_VISIBLE);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            is_fullscreen = 1;
        }
    } else {
        SetWindowLongA(hwnd, GWL_STYLE, saved_style);
        SetWindowPos(hwnd, NULL, saved_rect.left, saved_rect.top, saved_rect.right - saved_rect.left,
                     saved_rect.bottom - saved_rect.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOZORDER);
        is_fullscreen = 0;
    }
}

/* ---------------- audio: waveOut with a feeder thread ---------------- */
#define NBUF 4
#define BUF_FRAMES 1024
static HWAVEOUT wout;
static WAVEHDR whdr[NBUF];
static short wbuf[NBUF][BUF_FRAMES * 2];
static HANDLE wevent, wthread;
static volatile LONG audio_run;
static AudioCallback audio_cb;
static CRITICAL_SECTION audio_cs;
static int cs_init;

static DWORD WINAPI audio_main(LPVOID arg)
{
    int i;
    (void)arg;
    for (i = 0; i < NBUF; i++) {
        audio_cb(wbuf[i], BUF_FRAMES);
        waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
    }
    while (audio_run) {
        WaitForSingleObject(wevent, 50);
        for (i = 0; i < NBUF; i++) {
            if (whdr[i].dwFlags & WHDR_DONE) {
                audio_cb(wbuf[i], BUF_FRAMES);
                whdr[i].dwFlags &= ~WHDR_DONE;
                waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
            }
        }
    }
    return 0;
}

int plat_audio_init(int rate, AudioCallback cb)
{
    WAVEFORMATEX fmt;
    int i;
    if (!cs_init) { InitializeCriticalSection(&audio_cs); cs_init = 1; }
    memset(&fmt, 0, sizeof fmt);
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = (DWORD)rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = (DWORD)rate * 4;
    wevent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (waveOutOpen(&wout, WAVE_MAPPER, &fmt, (DWORD_PTR)wevent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(wevent); wevent = NULL; wout = NULL;
        return 0;
    }
    for (i = 0; i < NBUF; i++) {
        memset(&whdr[i], 0, sizeof(WAVEHDR));
        whdr[i].lpData = (LPSTR)wbuf[i];
        whdr[i].dwBufferLength = BUF_FRAMES * 4;
        waveOutPrepareHeader(wout, &whdr[i], sizeof(WAVEHDR));
    }
    audio_cb = cb;
    audio_run = 1;
    wthread = CreateThread(NULL, 0, audio_main, NULL, 0, NULL);
    return wthread != NULL;
}

void plat_audio_shutdown(void)
{
    int i;
    if (!wout) return;
    audio_run = 0;
    if (wthread) { WaitForSingleObject(wthread, 1000); CloseHandle(wthread); wthread = NULL; }
    waveOutReset(wout);
    for (i = 0; i < NBUF; i++) waveOutUnprepareHeader(wout, &whdr[i], sizeof(WAVEHDR));
    waveOutClose(wout);
    wout = NULL;
    if (wevent) { CloseHandle(wevent); wevent = NULL; }
}

void plat_lock(void)
{
    if (!cs_init) { InitializeCriticalSection(&audio_cs); cs_init = 1; }
    EnterCriticalSection(&audio_cs);
}
void plat_unlock(void) { LeaveCriticalSection(&audio_cs); }
