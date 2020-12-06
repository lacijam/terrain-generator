#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <stdio.h>

#include "opengl_loader.h"

#include "matrix.h"
#include "v3.h"
#include "camera.h"

#define KEY_DOWN 0xF000

// Creates a dummy window and gl context to load extensions.
void win32_init_opengl_extensions()
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

	load_gl_extensions();
	if (!wgl_is_supported("WGL_ARB_create_context")) {
		MessageBoxA(0, "Something went wrong during OpenGL extension loading!", "Fatal Error", 0);
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy_context);
	ReleaseDC(dummy_window, dummy_dc);
	DestroyWindow(dummy_window);
}

// TODO: This is redundant atm, vertex_shader and fragment_shader
// 		 are deleted when successfully linked to program.
struct ShaderProgram {
	unsigned id;
	unsigned vertex_shader;
	unsigned fragment_shader;
};

struct GameData {
	Camera *cur_cam;
	bool running;
	bool has_capture;
	bool has_focus;
	unsigned vao, vbo, ebo;
	ShaderProgram program;
	HGLRC gl_context;
};

void APIENTRY  
MessageCallback(GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam)
{
	fprintf(stderr, ">>>GLCALLBACK: %s id=%u %s\n",
           (type == GL_DEBUG_TYPE_ERROR ? "ERROR" : "" ),
            id, message);
}

void win32_create_gl_context(GameData *data, HWND hwnd)
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

	data->gl_context = wglCreateContextAttribsARB(dc, 0, attribs);

	if (!wglMakeCurrent(dc, data->gl_context)) {
		MessageBoxA(0, "Something went wrong during OpenGL 3.3 context current", "Fatal Error", 0);
	}

	#ifdef _DEBUG
		printf("Created OpenGL context, version=%s\nEnabling debug output\n", glGetString(GL_VERSION));
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(MessageCallback, 0);
	#endif

	glEnable(GL_DEPTH_TEST);

	wglSwapIntervalEXT(0);
}

void win32_on_create(GameData *data, HWND hwnd)
{
	win32_init_opengl_extensions();
	win32_create_gl_context(data, hwnd);

	data->cur_cam = camera_create();
	data->has_capture = false;
	data->has_focus = true;
	data->running = true;
}

void win32_on_destroy(GameData *data)
{
	camera_destroy(data->cur_cam);

	glBindVertexArray(0);

	glDeleteBuffers(1, &data->ebo);
	glDeleteBuffers(1, &data->vbo);
	glDeleteVertexArrays(1, &data->vao);
    glDeleteProgram(data->program.id);

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(data->gl_context);

	data->running = false;
}

void win32_reset_cursor_pos(HWND hwnd)
{
	RECT r;
	POINT p;
    GetClientRect(hwnd, &r);
	p.x = r.right / 2;
	p.y = r.bottom / 2;
	ClientToScreen(hwnd, &p);
	SetCursorPos(p.x, p.y);
}

bool gl_check_shader_compile_log(unsigned shader)
{
	int success;
	char info_log[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(shader, 512, 0, info_log);
		printf(">>>Shader compilation error: %s", info_log);
	};

	return success;
}

bool gl_check_program_link_log(unsigned program)
{
	int success;
	char info_log[512];
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, 512, NULL, info_log);
		printf(">>>Shader program linking error: %s", info_log);
	}

	return success;
}

bool gl_create_and_attach_shader(unsigned program_id, const char* src, GLenum shader_type)
{
    unsigned shader = glCreateShader(shader_type);
	if (!shader) {
		return false;
	}
	
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);
    glAttachShader(program_id, shader);
    if (!gl_check_shader_compile_log(shader)) {
        return false;
    }

    return true;
}

LRESULT handle_message(GameData* data, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		win32_on_destroy(data);	
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE: {
		int cx, cy;
		cx = LOWORD(lParam);
		cy = HIWORD(lParam);

		glViewport(0, 0, cx, cy);

		camera_frustrum(data->cur_cam, cx, cy);
	} break;

	case WM_CAPTURECHANGED: {
		SetCursor((HCURSOR)LoadImage(0, IDC_ARROW, IMAGE_CURSOR, 0, 0, 0));
		data->has_capture = false;
	} break;

	case WM_LBUTTONDOWN: {
		if (GetCapture() != hwnd) {
			SetCapture(hwnd);
			SetCursor(0);
			win32_reset_cursor_pos(hwnd);
			data->has_capture = true;
		}
	} break;

	// Handle this here to keep hwnd out of game logic.
	// Q will not quit out in the future.
	case WM_KEYDOWN: {
		if (wParam == 'Q') {
			PostMessage(hwnd, WM_CLOSE, 0, 0);
		}
	} break;

	case WM_SETFOCUS: {
		data->has_focus = true;
	} break;

	case WM_KILLFOCUS: {
		ReleaseCapture();
		data->has_focus = false;
	} break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return TRUE;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE) {
		CREATESTRUCT* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
		GameData* data = reinterpret_cast<GameData*>(create_struct->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);

		win32_on_create(data, hwnd);
	}

	LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	GameData* data = reinterpret_cast<GameData*>(ptr);
	if (data) {
		return handle_message(data, hwnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	// Allocate a new console and redirect stdout/in to it.
#ifdef _DEBUG
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
#endif

	GameData data;

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
		, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), &data
	);

	if (!hwnd) {
		MessageBoxA(0, "Failed to create window", "Fatal Error", 0);
		return 1;
	}

	const char* vertex_source = R"(
		#version 330

		in vec3 a_pos;
		out vec3 v_pos;

		uniform mat4 transform;

		void main()
		{
			v_pos = a_pos;
			gl_Position = transform * vec4(a_pos, 1.0);
		}
	)";
	
	const char* fragment_source = R"(
		#version 330
		
		in vec3 v_pos;

		out vec4 fragment;

		void main()
		{
			fragment = vec4(v_pos.x, v_pos.x, v_pos.x, 1.0);
		}
	)";

	float vertices[9] = {
		-0.5f,  -0.5f,  0.0f,
		 0.5f,  -0.5f,  0.0f,
		 0.0f,   0.5f,  0.0f
	};

	unsigned indices[3] {
		0, 1, 2
	};

	data.program.id = glCreateProgram();
	gl_create_and_attach_shader(data.program.id, vertex_source, GL_VERTEX_SHADER);
	gl_create_and_attach_shader(data.program.id, fragment_source, GL_FRAGMENT_SHADER);
	glLinkProgram(data.program.id);
    glUseProgram(data.program.id);
    if (!gl_check_program_link_log(data.program.id)) {
		MessageBoxA(0, "Something went wrong during shader program linking!", "Fatal Error", 0);
		return 1;
	}

	unsigned a_pos = glGetAttribLocation(data.program.id, "a_pos");
	unsigned transform_loc = glGetUniformLocation(data.program.id, "transform");

	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	glGenBuffers(1, &data.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, data.vbo);
	glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &data.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(unsigned), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(a_pos);

	ShowWindow(hwnd, nShowCmd);

	float model[16];
    Matrix::identity(model);

	double dt = 0;

	MSG msg = {};
	while (data.running) {
		LARGE_INTEGER start, freq;
		QueryPerformanceCounter(&start);
		QueryPerformanceFrequency(&freq);

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (data.has_focus) {
			if (data.has_capture) {
				POINT p;
				GetCursorPos(&p);
				ScreenToClient(hwnd, &p);

				camera_update(data.cur_cam, p.x, p.y);

				win32_reset_cursor_pos(hwnd);
			}
	
			if (GetKeyState('Q') & KEY_DOWN) {
			} else if (GetKeyState(VK_ESCAPE) & KEY_DOWN) {
				ReleaseCapture();
			} else {
				if (GetKeyState('W') & KEY_DOWN) {
					camera_move_forward(data.cur_cam, dt);
				} 
				
				else if (GetKeyState('S') & KEY_DOWN) {
					camera_move_backward(data.cur_cam, dt);
				} 
				
				if (GetKeyState('A')  & KEY_DOWN) {
					camera_move_left(data.cur_cam, dt);
				} else if (GetKeyState('D') & KEY_DOWN) {
					camera_move_right(data.cur_cam, dt);
				}
			}

			camera_look_at(data.cur_cam);

			glUseProgram(data.program.id);
			
			glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			for (unsigned i = 0; i < 1000; i += 10) {
				for (unsigned j = 0; j < 1000; j += 10) {
					Matrix::identity(model);
					Matrix::translate(model, j, 0.f, i);
					Matrix::rotate_x(model, 90);
					Matrix::scale(model, 10.f, 10.f, 10.f);

					float mv[16], mvp[16];
					Matrix::identity(mv);
					Matrix::identity(mvp);
					Matrix::multiply(mv, data.cur_cam->view, model);
					Matrix::multiply(mvp, data.cur_cam->frustrum, mv);
					glUniformMatrix4fv(transform_loc, 1, GL_FALSE, mvp);

					glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);
				}
			}

			for (unsigned i = 0; i < 1000; i += 10) {
				for (unsigned j = 0; j < 1000; j += 10) {
					Matrix::identity(model);
					Matrix::translate(model, j + 5, 0.f, i);
					Matrix::rotate_y(model, 180);
					Matrix::rotate_x(model, 90);
					Matrix::scale(model, 10.f, 10.f, 10.f);

					float mv[16], mvp[16];
					Matrix::identity(mv);
					Matrix::identity(mvp);
					Matrix::multiply(mv, data.cur_cam->view, model);
					Matrix::multiply(mvp, data.cur_cam->frustrum, mv);
					glUniformMatrix4fv(transform_loc, 1, GL_FALSE, mvp);

					glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);
				}
			}

			SwapBuffers(wglGetCurrentDC());
		
			glUseProgram(0);

			LARGE_INTEGER finish, elapsed;
			QueryPerformanceCounter(&finish);
			elapsed.QuadPart = finish.QuadPart - start.QuadPart;
			elapsed.QuadPart *= 1000000;
			elapsed.QuadPart /= freq.QuadPart;
			dt = elapsed.QuadPart / 1000000.;
		} else if (data.running) {
			WaitMessage();
		}
	}

	return 0;
}