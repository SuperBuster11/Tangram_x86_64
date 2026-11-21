// Linux window layer (Xlib): opens a scaled window, feeds Events to tg::Game at 50 Hz.
// usage: tangram_x11 [game.adf] [--scale N] [--frames N [--shot out.png] [--wav out.wav]] [--audio-stdout]
//   F7 cycles the window size (960x600, 1280x800, ... up to the screen); F8 cycles the filter (off / sharper / smoothed)
//   --frames: exit after N frames, optionally saving a PNG and the audio as a WAV file
//   --audio-stdout: write the audio as raw 16-bit stereo 44100 Hz to stdout (play with: ... | aplay -f S16_LE -r 44100 -c 2)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <fstream>
#include <iterator>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include "embedded_assets.hpp"
#include "game.hpp"
#include "icon_data.hpp"
#include "scale_filter.hpp"

int main(int argc, char** argv) {
    // The game data is compiled in; a disk image given as the first argument (not starting with "--") overrides it.
    const char* adfPath = argc >= 2 && std::strncmp(argv[1], "--", 2) ? argv[1] : nullptr;
    int scale = 3, maxFrames = -1; const char* shot = nullptr; const char* wavPath = nullptr; bool audioStdout = false;
    for (int i = adfPath ? 2 : 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--scale") && i + 1 < argc) scale = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) maxFrames = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wavPath = argv[++i];
        else if (!std::strcmp(argv[i], "--audio-stdout")) audioStdout = true;
    }
    tg::Adf adf; std::string err;
    if (adfPath) { if (!adf.loadFile(adfPath, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; } }
    else tg::loadEmbeddedAssets(adf);
    tg::Game game;
    if (!game.init(adf, err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const char* scorePath = "tangram.sco";   // the "scoredisk": high-score table kept in the current directory
    { std::ifstream in(scorePath, std::ios::binary); if (in) game.loadScores(tg::Bytes(std::istreambuf_iterator<char>(in), {})); }

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { std::fprintf(stderr, "cannot open X display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    // F7 cycles the window size through 960x600, 1280x800, ... up to the largest multiple of 320x200 that fits the screen
    int maxScale = 2;
    while (320 * (maxScale + 1) <= DisplayWidth(dpy, scr) && 200 * (maxScale + 1) <= DisplayHeight(dpy, scr) - 80) maxScale++;
    const int minScale = maxScale >= 3 ? 3 : 2;   // 960x600 is the smallest size (unless the screen is too small)
    scale = std::max(minScale, std::min(scale, maxScale));
    tg::ScaleFilter filter = tg::ScaleFilter::Off;
    int W = 320 * scale, H = 200 * scale;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), (DisplayWidth(dpy, scr) - W) / 2, (DisplayHeight(dpy, scr) - H) / 2, W, H, 0, 0, BlackPixel(dpy, scr));   // centred on the screen
    XStoreName(dpy, win, "Tangram");
    {   // window icon (_NET_WM_ICON: width, height, then ARGB pixels as longs)
        std::vector<unsigned long> ic; ic.push_back(64); ic.push_back(64);
        for (uint32_t v : kIcon64) ic.push_back(v);
        XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_ICON", False), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<unsigned char*>(ic.data()), int(ic.size()));
    }
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    Atom del = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &del, 1);
    XMapWindow(dpy, win);
    Visual* vis = DefaultVisual(dpy, scr);
    int depth = DefaultDepth(dpy, scr);
    if (depth < 24) { std::fprintf(stderr, "need a 24/32-bit display\n"); return 1; }
    std::vector<uint32_t> pix(size_t(W) * H);
    XImage* img = XCreateImage(dpy, vis, depth, ZPixmap, 0, reinterpret_cast<char*>(pix.data()), W, H, 32, 0);
    GC gc = XCreateGC(dpy, win, 0, nullptr);

    std::vector<int16_t> pcm, chunk(2 * 882);   // 882 stereo frames = 20 ms at 44100 Hz
    auto next = std::chrono::steady_clock::now();
    int frames = 0;
    bool running = true;
    while (running) {
        while (XPending(dpy)) {
            XEvent ev; XNextEvent(dpy, &ev);
            tg::Event e;
            if (ev.type == ClientMessage && Atom(ev.xclient.data.l[0]) == del) running = false;
            else if (ev.type == KeyPress && XLookupKeysym(&ev.xkey, 0) == XK_F7) {   // next size, back to the smallest after the largest
                scale = scale >= maxScale ? minScale : scale + 1;
                W = 320 * scale; H = 200 * scale;
                img->data = nullptr; XDestroyImage(img);
                pix.assign(size_t(W) * H, 0);
                img = XCreateImage(dpy, vis, depth, ZPixmap, 0, reinterpret_cast<char*>(pix.data()), W, H, 32, 0);
                XResizeWindow(dpy, win, unsigned(W), unsigned(H));
                XMoveWindow(dpy, win, std::max(0, (DisplayWidth(dpy, scr) - W) / 2), std::max(0, (DisplayHeight(dpy, scr) - H) / 2));
            }
            else if (ev.type == KeyPress && XLookupKeysym(&ev.xkey, 0) == XK_F8) filter = tg::nextFilter(filter);
            else if (ev.type == MotionNotify) { e.type = tg::Event::Move; e.x = ev.xmotion.x / scale; e.y = ev.xmotion.y / scale; game.event(e); }
            else if (ev.type == ButtonPress || ev.type == ButtonRelease) {
                if (ev.xbutton.button != 1 && ev.xbutton.button != 3) continue;
                e.type = ev.type == ButtonPress ? tg::Event::Down : tg::Event::Up;
                e.x = ev.xbutton.x / scale; e.y = ev.xbutton.y / scale; e.button = ev.xbutton.button == 3;
                game.event(e);
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                e.type = tg::Event::Key;
                if (ks == XK_Return || ks == XK_KP_Enter) e.key = tg::kKeyEnter;
                else if (ks == XK_Escape) e.key = tg::kKeyEsc;
                else if (ks == XK_BackSpace) e.key = tg::kKeyBackspace;
                else if (ks == XK_space) e.key = ' ';
                else if (ks >= XK_F1 && ks <= XK_F10) e.key = tg::kKeyF1 + int(ks - XK_F1);
                else if (ks >= XK_0 && ks <= XK_9) e.key = int(ks);
                else if (ks >= XK_a && ks <= XK_z) e.key = int(ks - XK_a + 'A');
                else if (ks >= XK_A && ks <= XK_Z) e.key = int(ks - XK_A + 'A');
                else {   // punctuation for the high-score name (the keyboard's own character)
                    char buf[8]; int n = XLookupString(&ev.xkey, buf, sizeof buf, nullptr, nullptr);
                    if (n != 1 || buf[0] < 33 || buf[0] > 95) continue;
                    e.key = buf[0];
                }
                game.event(e);
            }
        }
        game.step();
        if (game.takeScoresDirty()) { tg::Bytes d = game.saveScores(); std::ofstream out(scorePath, std::ios::binary); out.write(reinterpret_cast<const char*>(d.data()), std::streamsize(d.size())); }
        if (game.quitRequested()) break;
        if (wavPath || audioStdout) {
            game.audio(chunk.data(), 882);
            if (wavPath) pcm.insert(pcm.end(), chunk.begin(), chunk.end());
            if (audioStdout) std::fwrite(chunk.data(), 2, chunk.size(), stdout);
        }
        const tg::Image& f = game.frame();
        tg::scaleFrame(f, pix.data(), scale, filter);
        XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
        XFlush(dpy);
        if (maxFrames >= 0 && ++frames >= maxFrames) {
            if (shot) tg::writePng(shot, f, 1);
            if (wavPath) {
                std::FILE* w = std::fopen(wavPath, "wb");
                if (w) {
                    auto le32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) std::fputc(int((v >> (8 * i)) & 255), w); };
                    auto le16 = [&](uint16_t v) { std::fputc(v & 255, w); std::fputc(v >> 8, w); };
                    std::fwrite("RIFF", 1, 4, w); le32(uint32_t(36 + pcm.size() * 2)); std::fwrite("WAVEfmt ", 1, 8, w); le32(16); le16(1); le16(2);
                    le32(44100); le32(44100 * 4); le16(4); le16(16); std::fwrite("data", 1, 4, w); le32(uint32_t(pcm.size() * 2));
                    std::fwrite(pcm.data(), 2, pcm.size(), w); std::fclose(w);
                }
            }
            break;
        }
        next += std::chrono::milliseconds(20);   // 50 Hz
        std::this_thread::sleep_until(next);
    }
    img->data = nullptr;   // buffer is owned by `pix`
    XDestroyImage(img);
    XCloseDisplay(dpy);
    return 0;
}
