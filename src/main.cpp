#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <iostream>
#include <memory>

#include "opengl_loader.h"

#include "matrix.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define KEY_DOWN 0xF000

struct GameData {
	HWND hwnd;
	HCURSOR cursor;
	bool running;
	float view[16];
	float frustrum[16];
	float FOV_Y;
	float NEAR_CLIP;
	float FAR_CLIP;
	float yaw, pitch;
	Matrix::v3 camera_pos;
	Matrix::v3 camera_front;
	Matrix::v3 camera_up;
};

void win32_get_client_size(HWND hwnd, int *w, int *h)
{
    RECT r;
    GetClientRect(hwnd, &r);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}

void win32_get_window_centre(HWND hwnd, int *x, int *y)
{
	int cx, cy;
    RECT r;
	GetWindowRect(hwnd, &r);
	win32_get_client_size(hwnd, &cx, &cy);
	*x = r.left + cx / 2;
	*y = r.top + cy / 2;
}

void win32_reset_cursor_pos(HWND hwnd)
{
	int x, y;
	win32_get_window_centre(hwnd, &x, &y);
	SetCursorPos(x, y);
}

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

bool check_shader_compile_log(unsigned shader)
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

bool check_program_link_log(unsigned program)
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

bool create_and_attach_shader(unsigned program_id, const char* src, GLenum shader_type)
{
    unsigned shader = glCreateShader(shader_type);
	if (!shader) {
		return false;
	}
	
    glShaderSource(shader, 1, &src, 0);
    glCompileShader(shader);
    glAttachShader(program_id, shader);
    if (!check_shader_compile_log(shader)) {
        return false;
    }

    return true;
}

// TODO: This is redundant atm, vertex_shader and fragment_shader
// 		 are deleted when successfully linked to program.
struct ShaderProgram {
	unsigned id;
	unsigned vertex_shader;
	unsigned fragment_shader;
};

LRESULT handle_message(GameData* data, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		data->running = false;
		return 0;

	case WM_SIZE: {
		int cx, cy;
		cx = LOWORD(lParam);
		cy = HIWORD(lParam);

		glViewport(0, 0, cx, cy);
		data->FOV_Y = (90.f * (float)M_PI / 180.f);
		data->NEAR_CLIP = 0.01f;
		data->FAR_CLIP = 100.f;
		float aspect = (float)cx / cy;
		float top = data->NEAR_CLIP * tanf(data->FOV_Y / 2);
		float bottom = -1 * top;
		float left = bottom * aspect;
		float right = top * aspect;
		Matrix::frustrum(data->frustrum, left, right, bottom, top, data->NEAR_CLIP, data->FAR_CLIP);
	} break;

	case WM_CAPTURECHANGED: {
		SetCursor(LoadCursor(0, IDC_ARROW));
	} break;

	case WM_LBUTTONDOWN: {
		if (GetCapture() != data->hwnd) {
			SetCapture(data->hwnd);
			SetCursor(0);
			win32_reset_cursor_pos(data->hwnd);
		}
	} break;

	case WM_KILLFOCUS: {
		ReleaseCapture();
	} break;

	default:
		return DefWindowProc(data->hwnd, uMsg, wParam, lParam);
	}

	return TRUE;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE) {
		CREATESTRUCT* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
		GameData* data = reinterpret_cast<GameData*>(create_struct->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
	}

	LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	GameData* data = reinterpret_cast<GameData*>(ptr);
	if (data) {
		return handle_message(data, uMsg, wParam, lParam);
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Creates a dummy window and gl context to load extensions.
void init_opengl_extensions()
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
		exit(0);
	}

	load_gl_extensions();
	if (!wgl_is_supported("WGL_ARB_create_context")) {
		MessageBoxA(0, "Something went wrong during OpenGL extension loading!", "Fatal Error", 0);
		exit(0);
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(dummy_context);
	ReleaseDC(dummy_window, dummy_dc);
	DestroyWindow(dummy_window);
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

	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.lpfnWndProc = window_proc;
	window_class.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	window_class.hInstance = GetModuleHandle(NULL);
	window_class.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.lpszClassName = L"Game";
	RegisterClassEx(&window_class);

	data.hwnd = CreateWindowEx(
		NULL, L"Game", L"Game", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT
		, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), &data
	);

	init_opengl_extensions();

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

	HDC dc = GetDC(data.hwnd);
	int pixel_format;
	UINT num_formats;
	wglChoosePixelFormatARB(dc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
	if (!num_formats) {
		MessageBoxA(0, "Failed to choose a valid pixel format", "Fatal Error", 0);
		exit(0);
	}

	PIXELFORMATDESCRIPTOR pfd;
	DescribePixelFormat(dc, pixel_format, sizeof(pfd), &pfd);
	if (!SetPixelFormat(dc, pixel_format, &pfd)) {
		MessageBoxA(0, "Failed to set a pixel format", "Fatal Error", 0);
		exit(0);
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
		exit(0);
	}

	#ifdef _DEBUG
		printf("Created OpenGL context, version=%s\nEnabling debug output\n", glGetString(GL_VERSION));
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(MessageCallback, 0);
	#endif

	wglSwapIntervalEXT(1);

	glEnable(GL_DEPTH_TEST);

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
			fragment = vec4(v_pos.x, v_pos.y, v_pos.z, 1.0);
		}
	)";

	ShaderProgram program;
	program.id = glCreateProgram();
	create_and_attach_shader(program.id, vertex_source, GL_VERTEX_SHADER);
	create_and_attach_shader(program.id, fragment_source, GL_FRAGMENT_SHADER);
	glLinkProgram(program.id);
    glUseProgram(program.id);
    if (!check_program_link_log(program.id)) {
		MessageBoxA(0, "Something went wrong during shader program linking!", "Fatal Error", 0);
		exit(0);
	}

	unsigned a_pos = glGetAttribLocation(program.id, "a_pos");
	unsigned transform_loc = glGetUniformLocation(program.id, "transform");

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "res/teapot.obj");

	if (!warn.empty()) {
		std::cout << warn << std::endl;
	}
	if (!err.empty()) {
		std::cout << err << std::endl;
	}
	if (!success) {
		std::cout << "tinyobj::LoadObj something went wrong" << std::endl;
		return 0;
	}
	
	unsigned vao, vbo, ebo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, attrib.vertices.size() * sizeof(float), &attrib.vertices[0], GL_STATIC_DRAW);
	
	unsigned int* indices = (unsigned int*)malloc(shapes[0].mesh.indices.size() * sizeof(unsigned int));
	for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
		indices[i] = shapes[0].mesh.indices[i].vertex_index;
	}

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, shapes[0].mesh.indices.size() * sizeof(unsigned int), indices, GL_STATIC_DRAW);

	free(indices);

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(a_pos);

	ShowWindow(data.hwnd, nShowCmd);

	SetCapture(data.hwnd);
	SetCursor(0);

	float model[16];
    Matrix::identity(model);
	Matrix::scale(model, 1.f, 1.f, 1.f);
	Matrix::translate(model, 0.f, -1.f, -10.f);

    Matrix::identity(data.view);
	data.camera_pos = {0.f, 0.f, 0.f};
	data.camera_front = {0.f, 0.f, -1.f};
	data.camera_up = {0.f, 1.f, 0.f};
	data.yaw = 0;
	data.pitch = 0;

	data.running = true;
	MSG msg = {};
	while (data.running) {
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} 

		if (msg.message == WM_QUIT) {
			data.running = false;
		}
		
		if (GetCapture() == data.hwnd)
		{
			POINT p;
			GetCursorPos(&p);

			win32_reset_cursor_pos(data.hwnd);

			int mx, my;
			win32_get_window_centre(data.hwnd, &mx, &my);

			float x_off = p.x - mx;
			float y_off = my - p.y;

			data.yaw += x_off * 0.1f;
			data.pitch += y_off * 0.1f;
			if (data.pitch > 89.f) {
				data.pitch = 89.f;
			} else if (data.pitch <= -89.f) {
				data.pitch = -89.f;
			}

			Matrix::v3 direction;
			direction.x = cosf(Matrix::radians(data.yaw)) * cosf(Matrix::radians(data.pitch));
			direction.y = sinf(Matrix::radians(data.pitch));
			direction.z = sinf(Matrix::radians(data.yaw)) * cosf(Matrix::radians(data.pitch));
			data.camera_front = Matrix::normalise(direction);
		}
	
		// MOVE THIS MOVE THIS MOVE THIS
		// MOVEMENT CODE
		// MOVE THIS MOVE THIS MOVE THIS
		const float camera_speed = 1.f;
		if (GetKeyState('Q') & KEY_DOWN) {
			data.running = false;
		} 

		if (GetKeyState(VK_ESCAPE) & KEY_DOWN) {
			ReleaseCapture();
		}
		
		if (GetKeyState('W') & KEY_DOWN) {
			data.camera_pos += camera_speed * data.camera_front;
		} else if (GetKeyState('S') & KEY_DOWN) {
			data.camera_pos -= camera_speed * data.camera_front;
		} 
		
		if (GetKeyState('A')  & KEY_DOWN) {
			float x1, y1, z1;
			x1 = data.camera_front.x;
			y1 = data.camera_front.y;
			z1 = data.camera_front.z;
			float x2, y2, z2;
			x2 = data.camera_up.x;
			y2 = data.camera_up.y;
			z2 = data.camera_up.z;
			data.camera_pos -= Matrix::normalise(Matrix::cross(x1, y1, z1, x2, y2, z2)) * camera_speed;
		} else if (GetKeyState('D') & KEY_DOWN) {
			float x1, y1, z1;
			x1 = data.camera_front.x;
			y1 = data.camera_front.y;
			z1 = data.camera_front.z;
			float x2, y2, z2;
			x2 = data.camera_up.x;
			y2 = data.camera_up.y;
			z2 = data.camera_up.z;
			data.camera_pos += Matrix::normalise(Matrix::cross(x1, y1, z1, x2, y2, z2)) * camera_speed;
		}
		//

		glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(program.id);

		float i = -1.f;
		Matrix::rotate_x(model, i);

		Matrix::look_at(data.view, data.camera_pos, data.camera_pos + data.camera_front, data.camera_up);

		float mv[16], mvp[16];
		Matrix::identity(mv);
		Matrix::identity(mvp);
		Matrix::multiply(mv, data.view, model);
		Matrix::multiply(mvp, data.frustrum, mv);
		glUniformMatrix4fv(transform_loc, 1, GL_FALSE, mvp);

		glDrawElements(GL_TRIANGLES, (GLsizei)shapes[0].mesh.indices.size(), GL_UNSIGNED_INT, NULL);

		glUseProgram(0);

		SwapBuffers(wglGetCurrentDC());
	}

	glBindVertexArray(0);

	glDeleteBuffers(1, &ebo);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program.id);

	return 0;
}