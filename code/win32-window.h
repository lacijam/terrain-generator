#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "opengl_loader.h"

struct Win32Window {
    HWND hwnd;
    HGLRC gl_context;

    bool recieved_quit;
    bool has_focus;
    bool has_capture;
};

bool win32_create_window(Win32Window *window_data, WNDPROC window_proc);
void win32_close_window(Win32Window *window_data);
void win32_reset_cursor_pos(Win32Window *window_data);
void win32_on_create(Win32Window *window, HWND hwnd);