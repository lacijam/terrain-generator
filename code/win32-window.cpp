#include "win32-window.h"

#include <assert.h>

#include "opengl.h"

// Creates a dummy window and gl context to load extensions.
static void win32_init_opengl_extensions()
{
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

	gl_load_extensions();
	if (!wgl_is_supported("WGL_ARB_create_context")) {
		MessageBoxA(0, "Something went wrong during OpenGL extension loading!", "Fatal Error", 0);
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy_context);
	ReleaseDC(dummy_window, dummy_dc);
	DestroyWindow(dummy_window);
}

static void win32_create_gl_context(Win32Window *window)
{
    assert(window && window->hwnd);

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

	HDC dc = GetDC(window->hwnd);
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

	window->gl_context = wglCreateContextAttribsARB(dc, 0, attribs);

	if (!wglMakeCurrent(dc, window->gl_context)) {
		MessageBoxA(0, "Something went wrong during OpenGL 3.3 context current", "Fatal Error", 0);
	}

	glEnable(GL_DEPTH_TEST);

	wglSwapIntervalEXT(0);
}

void win32_on_create(Win32Window *window, HWND hwnd)
{
    window->hwnd = hwnd;

	win32_init_opengl_extensions();
	win32_create_gl_context(window);

	window->has_capture = false;
	window->has_focus = true;
	window->recieved_quit = false;
}

void win32_close_window(Win32Window *window)
{
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(window->gl_context);

	window->recieved_quit = true;
}

void win32_reset_cursor_pos(Win32Window *window_data)
{
	RECT r;
	POINT p;
    GetClientRect(window_data->hwnd, &r);
	p.x = r.right / 2;
	p.y = r.bottom / 2;
	ClientToScreen(window_data->hwnd, &p);
	SetCursorPos(p.x, p.y);
}

bool win32_create_window(Win32Window *window_data, WNDPROC window_proc)
{
    WNDCLASSEXA window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.lpfnWndProc = window_proc;
	window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	window_class.hInstance = GetModuleHandle(0);
	window_class.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	window_class.hCursor = (HCURSOR)LoadImage(0, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
	window_class.lpszClassName = "Game";
	RegisterClassExA(&window_class);

	HWND hwnd = CreateWindowExA(
		NULL, "Game", "Game foo", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT
		, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), window_data
	);

	if (!hwnd) {
		MessageBoxA(0, "Failed to create window", "Fatal Error", 0);
		return false;
	}

    return true;
}