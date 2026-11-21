// Windows window layer (Win32 + GDI), same job as main_x11.cpp. NOT compiled or tested on Windows by the author.
// usage: tangram.exe [game.adf]   (default: disk.adf next to the exe)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>

#include <string>
#include <vector>

#include "embedded_assets.hpp"
#include "game.hpp"
#include "scale_filter.hpp"

static tg::Game g_game;
static int g_scale = 3;        // window size = 320x200 times this; F7 cycles 3 .. g_maxScale (960x600 and up)
static int g_maxScale = 2;
static tg::ScaleFilter g_filter = tg::ScaleFilter::Off;   // F8
static int minScale() { return g_maxScale >= 3 ? 3 : 2; }
static std::vector<uint32_t> g_pix;
static std::recursive_mutex g_lock;            // the game is touched by the window thread and the audio thread
static std::atomic<bool> g_quit{false};

// Audio: 4 buffers of 40 ms through waveOut, refilled by a thread that renders the original music player.
static void audioThread() {
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 2; fmt.nSamplesPerSec = 44100; fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = 44100 * 4;
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    HWAVEOUT out = nullptr;
    if (waveOutOpen(&out, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(ev), 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) return;
    constexpr int kBuffers = 4, kFrames = 1764;
    static short data[kBuffers][kFrames * 2];
    WAVEHDR hdr[kBuffers] = {};
    for (int i = 0; i < kBuffers; i++) {
        hdr[i].lpData = reinterpret_cast<LPSTR>(data[i]); hdr[i].dwBufferLength = sizeof data[i];
        waveOutPrepareHeader(out, &hdr[i], sizeof hdr[i]);
        hdr[i].dwFlags |= WHDR_DONE;   // treat as free
    }
    while (!g_quit) {
        for (int i = 0; i < kBuffers; i++) {
            if (!(hdr[i].dwFlags & WHDR_DONE)) continue;
            { std::lock_guard<std::recursive_mutex> l(g_lock); g_game.audio(data[i], kFrames); }
            hdr[i].dwFlags &= ~WHDR_DONE;
            waveOutWrite(out, &hdr[i], sizeof hdr[i]);
        }
        WaitForSingleObject(ev, 50);
    }
    waveOutReset(out);
    for (int i = 0; i < kBuffers; i++) waveOutUnprepareHeader(out, &hdr[i], sizeof hdr[i]);
    waveOutClose(out);
    CloseHandle(ev);
}

static RECT workArea() { RECT r; SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0); return r; }

// Outer size of a window whose client area is 320*n x 200*n.
static SIZE outerSize(int n) {
    RECT r = {0, 0, 320 * n, 200 * n};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    return SIZE{r.right - r.left, r.bottom - r.top};
}

static void computeMaxScale() {   // the largest size that still fits the work area of the screen
    RECT wa = workArea(); g_maxScale = 2;
    for (int n = 3;; n++) {
        SIZE s = outerSize(n);
        if (s.cx > wa.right - wa.left || s.cy > wa.bottom - wa.top) break;
        g_maxScale = n;
    }
}

static void setScale(HWND h, int n) {   // resizes the window and centres it in the work area
    g_scale = n;
    SIZE s = outerSize(n); RECT wa = workArea();
    SetWindowPos(h, nullptr, wa.left + (wa.right - wa.left - s.cx) / 2, wa.top + (wa.bottom - wa.top - s.cy) / 2, s.cx, s.cy, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void present(HDC dc, int cw, int ch) {
    const tg::Image& f = g_game.frame();
    const int n = std::max(1, std::min(cw / 320, ch / 200));   // integer factor of the window; the rest is stretched 1:1 -> exact for F7 sizes
    g_pix.resize(size_t(320 * n) * 200 * n);
    tg::scaleFrame(f, g_pix.data(), n, g_filter);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = 320 * n; bi.bmiHeader.biHeight = -200 * n;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, 0, 0, cw, ch, 0, 0, 320 * n, 200 * n, g_pix.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    RECT rc; GetClientRect(h, &rc);
    int cw = rc.right ? rc.right : 1, ch = rc.bottom ? rc.bottom : 1;
    auto mx = [&] { return int(short(LOWORD(l))) * 320 / cw; };
    auto my = [&] { return int(short(HIWORD(l))) * 200 / ch; };
    tg::Event e;
    switch (m) {
    case WM_MOUSEMOVE: e.type = tg::Event::Move; e.x = mx(); e.y = my(); g_game.event(e); return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_LBUTTONUP: case WM_RBUTTONUP:
        e.type = (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) ? tg::Event::Down : tg::Event::Up;
        e.x = mx(); e.y = my(); e.button = (m == WM_RBUTTONDOWN || m == WM_RBUTTONUP);
        if (e.type == tg::Event::Down) SetCapture(h); else ReleaseCapture();   // the release is delivered even outside the window
        g_game.event(e); return 0;
    case WM_SYSKEYDOWN:   // F10 arrives as a system key in Windows
        if (w != VK_F10) break;
        e.type = tg::Event::Key; e.key = tg::kKeyF1 + 9; g_game.event(e); return 0;
    case WM_KEYDOWN:
        if (w == VK_F7) { setScale(h, g_scale >= g_maxScale ? minScale() : g_scale + 1); return 0; }
        if (w == VK_F8) { g_filter = tg::nextFilter(g_filter); return 0; }   // filter: off / sharper / smoothed   // next window size
        e.type = tg::Event::Key;
        if (w == VK_RETURN) e.key = tg::kKeyEnter; else if (w == VK_ESCAPE) e.key = tg::kKeyEsc; else if (w == VK_BACK) e.key = tg::kKeyBackspace;
        else if (w == VK_SPACE) e.key = ' ';
        else if (w >= VK_F1 && w <= VK_F10) e.key = tg::kKeyF1 + int(w - VK_F1);
        else if (w >= '0' && w <= '9') e.key = int(w); else if (w >= 'A' && w <= 'Z') e.key = int(w);
        else if (w >= VK_NUMPAD0 && w <= VK_NUMPAD9) e.key = int('0' + (w - VK_NUMPAD0)); else break;
        g_game.event(e); return 0;
    case WM_CHAR:   // punctuation for the high-score name (letters, digits and space come from WM_KEYDOWN)
        if (w >= 33 && w <= 95 && !((w >= '0' && w <= '9') || (w >= 'A' && w <= 'Z'))) { e.type = tg::Event::Key; e.key = int(w); g_game.event(e); }
        return 0;
    case WM_TIMER: g_game.step();
        if (g_game.takeScoresDirty()) { tg::Bytes d = g_game.saveScores(); std::ofstream out("tangram.sco", std::ios::binary); out.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size())); } if (g_game.quitRequested()) { DestroyWindow(h); return 0; } InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); present(dc, cw, ch); EndPaint(h, &ps); return 0; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmd, int show) {
    // The game data is compiled in; a disk image path on the command line overrides it.
    std::string path = cmd ? cmd : "";
    if (path.size() > 1 && path.front() == '"' && path.back() == '"') path = path.substr(1, path.size() - 2);
    tg::Adf adf; std::string err;
    bool loaded = true;
    if (path.empty()) tg::loadEmbeddedAssets(adf); else loaded = adf.loadFile(path, err);
    if (!loaded || !g_game.init(adf, err)) { MessageBoxA(nullptr, err.c_str(), "Tangram", MB_ICONERROR); return 1; }
    { std::ifstream in("tangram.sco", std::ios::binary); if (in) g_game.loadScores(tg::Bytes(std::istreambuf_iterator<char>(in), {})); }
    WNDCLASSA wc = {}; wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = "TangramWnd"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));   // icon from assets/tangram.rc
    RegisterClassA(&wc);
    computeMaxScale();
    g_scale = std::max(minScale(), std::min(g_scale, g_maxScale));
    SIZE sz = outerSize(g_scale); RECT wa = workArea();
    HWND h = CreateWindowA("TangramWnd", "Tangram", WS_OVERLAPPEDWINDOW | WS_VISIBLE, wa.left + (wa.right - wa.left - sz.cx) / 2, wa.top + (wa.bottom - wa.top - sz.cy) / 2, sz.cx, sz.cy, nullptr, nullptr, inst, nullptr);
    ShowCursor(FALSE);
    timeBeginPeriod(1);
    SetTimer(h, 1, 20, nullptr);   // 50 Hz (the timer is coarse; good enough for a first version)
    std::thread audio(audioThread);
    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    g_quit = true;
    audio.join();
    return 0;
}
