#include "win32-opengl.h"

#include <stdio.h>
#include <stdlib.h>

#include "types.h"

#define GLF(name, uppername) PFNGL##uppername##PROC gl##name
GL_FUNCS;
#undef GLF

#define WGLF(name, uppername) PFNWGL##uppername##PROC wgl##name
WGL_FUNCS;
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

static bool32 win32_wgl_is_supported(const char *str)
{
    char *wext_str = _strdup(wglGetExtensionsStringARB(wglGetCurrentDC()));
    char *next = NULL;
    char *wext = strtok_s(wext_str, " ", &next);
    bool32 found = false;
    
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
void win32_init_opengl_extensions()
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

	s32 pixelFormatNumber = ChoosePixelFormat(dummy_dc, &pfd);
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

static void APIENTRY  
gl_message_callback(GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam)
{
	fprintf(stdout, ">>>GLCALLBACK: %s id=%u %s\n",
           (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "" ),
            id, message);
}

HGLRC win32_create_gl_context(HWND hwnd)
{
	INT pixel_format_attribs[] =
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
	INT pixel_format;
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

	INT attribs[] = {
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

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(gl_message_callback, 0);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);  
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	wglSwapIntervalEXT(0);

    return gl_context;
}