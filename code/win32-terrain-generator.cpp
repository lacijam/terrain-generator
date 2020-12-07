
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef int32_t bool32;

typedef float real32;
typedef double real64;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <assert.h>

#define _USE_MATH_DEFINES
#include <math.h>
#include "maths.cpp"
#include "camera.cpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <gl/GL.h>
#include "gl/glext.h"
#include "gl/wglext.h"

#include "opengl.cpp"
#include "app.cpp"


static bool window_resized;
static bool running;
static HGLRC glrc;

// wgl extensions.
#define WGLF(name, uppername) static PFNWGL##uppername##PROC wgl##name
#define WGL_FUNCS \
WGLF(GetExtensionsStringARB, GETEXTENSIONSSTRINGARB);\
WGLF(ChoosePixelFormatARB, CHOOSEPIXELFORMATARB);\
WGLF(CreateContextAttribsARB, CREATECONTEXTATTRIBSARB);\
WGLF(SwapIntervalEXT, SWAPINTERVALEXT);
WGL_FUNCS
#undef WGLF

static void *win32_get_any_gl_proc(const char *name)
{
    void *p = (void *)wglGetProcAddress(name);
    if (p == 0 ||
        (p == (void *)0x1) || (p == (void *)0x2) || (p == (void *)0x3) ||
        (p == (void *)-1) )
    {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void *)GetProcAddress(module, name);
    }

    return p;
}

static void win32_gl_load_extensions()
{
    #define GLF(name, uppername) gl##name = (PFNGL##uppername##PROC)win32_get_any_gl_proc("gl"#name"")
    GL_FUNCS
    #undef GLF

    #define WGLF(name, uppername) wgl##name = (PFNWGL##uppername##PROC)win32_get_any_gl_proc("wgl"#name"")
    WGL_FUNCS
    #undef WGLF
}

static bool win32_wgl_is_supported(const char *str)
{
    char *wext_str = _strdup(wglGetExtensionsStringARB(wglGetCurrentDC()));
    char *next = NULL;
    char *wext = strtok_s(wext_str, " ", &next);
    bool found = false;
    
    while (wext != NULL && !found) {
        wext = strtok_s(NULL, " ", &next);
        if (strcmp(wext, str)) {
            found = true;
        }
    }

    free(wext_str);

    return found;
}

// Creates a dummy window and gl context to load extensions.
static void win32_init_opengl_extensions()
{
	// @TODO: Do this on another thread and we don't need to create a dummy window?
	WNDCLASSA window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = DefWindowProc;
	window_class.hInstance = GetModuleHandle(0);
	window_class.lpszClassName = "dummy_class";
	window_class.hCursor = NULL;
	RegisterClassA(&window_class);

	HWND dummy_window = CreateWindowExA(
        0, window_class.lpszClassName, "dummy", 0,
        CW_USEDEFAULT, CW_USEDEFAULT,  CW_USEDEFAULT,  CW_USEDEFAULT,
        0, 0, window_class.hInstance, 0);

	HDC dummy_dc = GetDC(dummy_window);
	
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormatNumber = ChoosePixelFormat(dummy_dc, &pfd);
	SetPixelFormat(dummy_dc, pixelFormatNumber, &pfd);

	HGLRC dummy_context = wglCreateContext(dummy_dc);
	if (!wglMakeCurrent(dummy_dc, dummy_context)) {
		MessageBoxA(0, "Something went wrong during dummy context creation!", "Error", 0);
	}

	win32_gl_load_extensions();
	if (!win32_wgl_is_supported("WGL_ARB_create_context")) {
		MessageBoxA(0, "Something went wrong during OpenGL extension loading!", "Fatal Error", 0);
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy_context);
	ReleaseDC(dummy_window, dummy_dc);
	DestroyWindow(dummy_window);
}

static HGLRC win32_create_gl_context(HWND hwnd)
{
	int pixel_format_attribs[] =
	{
		WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
		WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB, 32,
		WGL_DEPTH_BITS_ARB, 24,
		WGL_STENCIL_BITS_ARB, 8,
		0
	};

	HDC dc = GetDC(hwnd);
	int pixel_format;
	UINT num_formats;
	wglChoosePixelFormatARB(dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
	if (!num_formats) {
		MessageBoxA(0, "Failed to choose a valid pixel format", "Fatal Error", 0);
	}

	PIXELFORMATDESCRIPTOR pfd;
	DescribePixelFormat(dc, pixel_format, sizeof(pfd), &pfd);
	if (!SetPixelFormat(dc, pixel_format, &pfd)) {
		MessageBoxA(0, "Failed to set a pixel format", "Fatal Error", 0);
	}

	int attribs[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		#ifdef _DEBUG
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
		#else
		WGL_CONTEXT_FLAGS_ARB, 0,
		#endif
		0
	};

	HGLRC gl_context = wglCreateContextAttribsARB(dc, 0, attribs);

	if (!wglMakeCurrent(dc, gl_context)) {
		MessageBoxA(0, "Something went wrong during OpenGL 3.3 context current", "Fatal Error", 0);
	}

	glEnable(GL_DEPTH_TEST);

	wglSwapIntervalEXT(0);

    return gl_context;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		running = false;
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
		window_resized = true;
		break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return TRUE;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASSEXA window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.lpfnWndProc = window_proc;
	window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	window_class.hInstance = GetModuleHandle(0);
	window_class.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	window_class.hCursor = (HCURSOR)LoadImage(0, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
	window_class.lpszClassName = "TerrainGenerator";
	RegisterClassExA(&window_class);

	HWND hwnd = CreateWindowExA(
		NULL, "TerrainGenerator", "Terrain Generator", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT
		, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), 0
	);

	if (!hwnd) {
		MessageBoxA(0, "Failed to create window", "Fatal Error", 0);
		return 1;
	}

	win32_init_opengl_extensions();
	glrc = win32_create_gl_context(hwnd);

    ShowWindow(hwnd, nShowCmd);

	game_memory memory = {};
	memory.permenant_storage_size = Megabytes(64);
	memory.permenant_storage = VirtualAlloc(0, memory.permenant_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	
	if (memory.permenant_storage) {
		double dt = 0;
		running = true;

		while (running) {
			MSG msg;

			LARGE_INTEGER start, freq;
			QueryPerformanceCounter(&start);
			QueryPerformanceFrequency(&freq);

			game_input input = {};
			game_window_info window_info = {};

			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					running = false;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			BYTE keys[256];
			GetKeyboardState(keys);
			input.keyboard.forward.ended_down = keys['W'] & 0x80;
			input.keyboard.backward.ended_down = keys['S'] & 0x80;
			input.keyboard.left.ended_down = keys['A'] & 0x80;
			input.keyboard.right.ended_down = keys['D'] & 0x80;
			input.keyboard.cam_up.ended_down = keys[VK_UP] & 0x80;
			input.keyboard.cam_down.ended_down = keys[VK_DOWN] & 0x80;
			input.keyboard.cam_left.ended_down = keys[VK_LEFT] & 0x80;
			input.keyboard.cam_right.ended_down = keys[VK_RIGHT] & 0x80;

			RECT client;
			GetClientRect(hwnd, &client);
			window_info.w = client.right;
			window_info.h = client.bottom;
			window_info.resize = window_resized;
			app_update_and_render(dt, &input, &memory, &window_info);
			window_resized = false;

			SwapBuffers(wglGetCurrentDC());
			
			LARGE_INTEGER finish, elapsed;
			QueryPerformanceCounter(&finish);
			elapsed.QuadPart = finish.QuadPart - start.QuadPart;
			elapsed.QuadPart *= 1000000;
			elapsed.QuadPart /= freq.QuadPart;
			dt = elapsed.QuadPart / 1000000.;
		}
	}

	VirtualFree(memory.permenant_storage, memory.permenant_storage_size, MEM_RELEASE);

    wglMakeCurrent(0, 0);
    wglDeleteContext(glrc);

    return 0;
}