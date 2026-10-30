/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Morten Lein */
/* Screenshots of the plugin's screen without a DAW: loads Ond.clap, opens its GUI in a window of its own, saves
 * it as PREFIX_0.bmp, then for each X,Y given (the screen's own 1000 x 640 coordinates) clicks there and saves
 * PREFIX_1.bmp, PREFIX_2.bmp ... The window shows for about a second a shot.
 *   clap_gui_shot PLUGIN.clap PREFIX [X,Y ...] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "entry.h"
#include "ext/gui.h"
#include "ext/timer-support.h"
#include "plugin-factory.h"
#include "host.h"

static void host_nop(const clap_host_t *h) { (void)h; }
static bool gui_resize(const clap_host_t *h, uint32_t w, uint32_t hh) { (void)h; (void)w; (void)hh; return false; }
static bool gui_bool(const clap_host_t *h) { (void)h; return true; }
static void gui_closed(const clap_host_t *h, bool d) { (void)h; (void)d; }
static const clap_host_gui_t HOST_GUI = {host_nop, gui_resize, gui_bool, gui_bool, gui_closed};
/* the plugin's timers, run from pump() (DPF's screen idles on them) */
static struct { clap_id id; uint32_t ms; DWORD last; } timers[8];
static uint32_t ntimers;
static const clap_plugin_t *timer_plugin;
static bool timer_reg(const clap_host_t *h, uint32_t ms, clap_id *id)
{
    (void)h;
    if (ntimers >= 8)
        return false;
    timers[ntimers].id = *id = ntimers, timers[ntimers].ms = ms ? ms : 16, timers[ntimers].last = GetTickCount();
    ntimers++;
    return true;
}
static bool timer_unreg(const clap_host_t *h, clap_id id) { (void)h; if (id < ntimers) timers[id].ms = 0; return true; }
static const clap_host_timer_support_t HOST_TIMER = {timer_reg, timer_unreg};
static const void *host_ext(const clap_host_t *h, const char *id)
{
    (void)h;
    if (!strcmp(id, CLAP_EXT_GUI))
        return &HOST_GUI;
    if (!strcmp(id, CLAP_EXT_TIMER_SUPPORT))
        return &HOST_TIMER;
    return NULL;
}
static const clap_host_t HOST = {CLAP_VERSION_INIT, NULL, "ond clap_gui_shot", "Ond", "", "0.1",
                                 host_ext, host_nop, host_nop, host_nop};

static void pump(DWORD ms)
{
    DWORD t0 = GetTickCount();
    MSG m;
    while (GetTickCount() - t0 < ms) {
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
        if (timer_plugin) {
            const clap_plugin_timer_support_t *ts = timer_plugin->get_extension(timer_plugin, CLAP_EXT_TIMER_SUPPORT);
            for (uint32_t i = 0; ts && i < ntimers; i++)
                if (timers[i].ms && GetTickCount() - timers[i].last >= timers[i].ms) {
                    timers[i].last = GetTickCount();
                    ts->on_timer(timer_plugin, timers[i].id);
                }
        }
        Sleep(10);
    }
}

static int save_bmp(HWND w, const char *path)
{
    RECT r;
    int width, height, ok;
    HDC sdc, mdc;
    HBITMAP bm;
    BITMAPINFOHEADER bi = {sizeof bi};
    BITMAPFILEHEADER bf = {0x4D42};
    void *bits;
    FILE *f;
    GetClientRect(w, &r);
    width = r.right, height = r.bottom;
    sdc = GetDC(NULL);
    mdc = CreateCompatibleDC(sdc);
    bi.biWidth = width, bi.biHeight = -height, bi.biPlanes = 1, bi.biBitCount = 32;
    bm = CreateDIBSection(sdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mdc, bm);
    {   /* the screen under the window: what is shown, GL included */
        POINT p = {0, 0};
        ClientToScreen(w, &p);
        ok = BitBlt(mdc, 0, 0, width, height, sdc, p.x, p.y, SRCCOPY);
    }
    bi.biHeight = height;
    bf.bfOffBits = sizeof bf + sizeof bi;
    bf.bfSize = bf.bfOffBits + (DWORD)(width * height * 4);
    if (!(f = fopen(path, "wb")))
        return 0;
    fwrite(&bf, sizeof bf, 1, f);
    fwrite(&bi, sizeof bi, 1, f);
    for (int y = height - 1; y >= 0; y--)
        fwrite((char *)bits + (size_t)y * width * 4, 4, (size_t)width, f);
    fclose(f);
    DeleteObject(bm);
    DeleteDC(mdc);
    ReleaseDC(NULL, sdc);
    printf("%s %dx%d%s\n", path, width, height, ok ? "" : " (capture failed)");
    return ok;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "Ond.clap", *prefix = argc > 2 ? argv[2] : "shot";
    HMODULE dll;
    const clap_plugin_entry_t *entry;
    const clap_plugin_factory_t *fac;
    const clap_plugin_t *pl;
    const clap_plugin_gui_t *gui;
    uint32_t w = 0, h = 0;
    HWND win, child;
    clap_window_t cw;
    char out[512];
    int k;
    SetProcessDPIAware();
    dll = LoadLibraryA(path);
    entry = dll ? (const clap_plugin_entry_t *)GetProcAddress(dll, "clap_entry") : NULL;
    if (!entry || !entry->init(path))
        return 1;
    fac = entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    pl = fac->create_plugin(fac, &HOST, fac->get_plugin_descriptor(fac, 0)->id);
    if (!pl || !pl->init(pl) || !pl->activate(pl, 48000, 1, 512))
        return 1;
    timer_plugin = pl;
    gui = pl->get_extension(pl, CLAP_EXT_GUI);
    if (!gui || !gui->create(pl, CLAP_WINDOW_API_WIN32, false) || !gui->get_size(pl, &w, &h)) {
        fprintf(stderr, "no embeddable GUI\n");
        return 1;
    }
    win = CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "Ond", WS_POPUP | WS_VISIBLE, 40, 40, (int)w, (int)h,
                          NULL, NULL, GetModuleHandleA(NULL), NULL);
    cw.api = CLAP_WINDOW_API_WIN32;
    cw.win32 = win;
    gui->set_parent(pl, &cw);
    gui->show(pl);
    SetWindowPos(win, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    pump(900);
    snprintf(out, sizeof out, "%s_0.bmp", prefix);
    save_bmp(win, out);
    child = GetWindow(win, GW_CHILD);
    for (k = 3; k < argc; k++) {
        int x, y;
        float sf = (float)w / 1000.f;
        if (sscanf(argv[k], "%d,%d", &x, &y) != 2)
            continue;
        x = (int)(x * sf), y = (int)(y * sf);
        PostMessageA(child ? child : win, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        pump(60);
        PostMessageA(child ? child : win, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
        pump(700);
        snprintf(out, sizeof out, "%s_%d.bmp", prefix, k - 2);
        save_bmp(win, out);
    }
    gui->destroy(pl);
    DestroyWindow(win);
    pl->deactivate(pl);
    pl->destroy(pl);
    entry->deinit();
    return 0;
}
