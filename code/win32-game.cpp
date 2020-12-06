#include <stdio.h>

#include "win32-window.h"
#include "opengl-util.h"
#include "matrix.h"
#include "v3.h"
#include "camera.h"

#define KEY_DOWN 0xF000

static Camera *cur_cam;
static unsigned vao, vbo, ebo;
static unsigned shader_program;
static unsigned transform_loc;
static float model[16];

void on_size(int cx, int cy)
{
	camera_frustrum(cur_cam, cx, cy);
}

void on_create()
{
	cur_cam = camera_create();

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

	shader_program = glCreateProgram();
	gl_create_and_attach_shader(shader_program, vertex_source, GL_VERTEX_SHADER);
	gl_create_and_attach_shader(shader_program, fragment_source, GL_FRAGMENT_SHADER);
	glLinkProgram(shader_program);
    glUseProgram(shader_program);
    if (!gl_check_program_link_log(shader_program)) {
		MessageBoxA(0, "Something went wrong during shader program linking!", "Fatal Error", 0);
	}

	unsigned a_pos = glGetAttribLocation(shader_program, "a_pos");
	unsigned transform_loc = glGetUniformLocation(shader_program, "transform");

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3 * sizeof(unsigned), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(a_pos);

    Matrix::identity(model);
}

void on_close()
{
	camera_destroy(cur_cam);

	glBindVertexArray(0);

	glDeleteBuffers(1, &ebo);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shader_program);
}

LRESULT handle_message(Win32Window* window, HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		win32_close_window(window);	
		on_close();
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

		on_size(cx, cy);
	} break;

	case WM_CAPTURECHANGED: {
		SetCursor((HCURSOR)LoadImage(0, IDC_ARROW, IMAGE_CURSOR, 0, 0, 0));
		window->has_capture = false;
	} break;

	case WM_LBUTTONDOWN: {
		if (GetCapture() != hwnd) {
			SetCapture(hwnd);
			SetCursor(0);
			win32_reset_cursor_pos(window);
			window->has_capture = true;
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
		window->has_focus = true;
	} break;

	case WM_KILLFOCUS: {
		ReleaseCapture();
		window->has_focus = false;
	} break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return TRUE;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE) {
		CREATESTRUCT* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
		Win32Window* window_data = reinterpret_cast<Win32Window*>(create_struct->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window_data);

		win32_on_create(window_data, hwnd);
		on_create();
	}

	LONG_PTR ptr = GetWindowLongPtr(hwnd, GWLP_USERDATA);
	Win32Window* window_data = reinterpret_cast<Win32Window*>(ptr);
	if (window_data) {
		return handle_message(window_data, hwnd, uMsg, wParam, lParam);
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

	Win32Window *game_window = (Win32Window*)HeapAlloc(GetProcessHeap(), 0, sizeof(Win32Window));
	win32_create_window(game_window, window_proc);

	#ifdef _DEBUG
		printf("Created OpenGL context, version=%s\nEnabling debug output\n", glGetString(GL_VERSION));
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_message_callback, 0);
	#endif

	ShowWindow(game_window->hwnd, nShowCmd);

	double dt = 0;

	MSG msg = {};
	while (!game_window->recieved_quit) {
		LARGE_INTEGER start, freq;
		QueryPerformanceCounter(&start);
		QueryPerformanceFrequency(&freq);

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (game_window->has_focus) {
			if (game_window->has_capture) {
				POINT p;
				GetCursorPos(&p);
				ScreenToClient(game_window->hwnd, &p);

				camera_update(cur_cam, p.x, p.y);

				win32_reset_cursor_pos(game_window);
			}
	
			if (GetKeyState('Q') & KEY_DOWN) {
			} else if (GetKeyState(VK_ESCAPE) & KEY_DOWN) {
				ReleaseCapture();
			} else {
				if (GetKeyState('W') & KEY_DOWN) {
					camera_move_forward(cur_cam, dt);
				} 
				
				else if (GetKeyState('S') & KEY_DOWN) {
					camera_move_backward(cur_cam, dt);
				} 
				
				if (GetKeyState('A')  & KEY_DOWN) {
					camera_move_left(cur_cam, dt);
				} else if (GetKeyState('D') & KEY_DOWN) {
					camera_move_right(cur_cam, dt);
				}
			}

			camera_look_at(cur_cam);

			glUseProgram(shader_program);
			
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
					Matrix::multiply(mv, cur_cam->view, model);
					Matrix::multiply(mvp, cur_cam->frustrum, mv);
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
					Matrix::multiply(mv, cur_cam->view, model);
					Matrix::multiply(mvp, cur_cam->frustrum, mv);
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
		} else if (!game_window->recieved_quit) {
			WaitMessage();
		}
	}

	HeapFree(GetProcessHeap(), 0, game_window);

	return 0;
}