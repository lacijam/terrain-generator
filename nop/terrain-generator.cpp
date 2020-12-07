#include <stdio.h>

#include "win32-window.h"
#include "opengl.h"
#include "matrix.h"
#include "v3.h"
#include "camera.h"

#include <assert.h>

#define KEY_DOWN 0xF000
#include <stdlib.h>
#include <time.h>

static Camera *cur_cam;
static unsigned vao, vbo, ebo;

static unsigned shader_program;
static unsigned lighting_shader_program;

static unsigned transform_loc;
static unsigned object_colour_loc;
static unsigned model_loc;
static unsigned light_pos_loc;

static const int chunk_width = 100;
static const int chunk_height = 100;

void on_size(int cx, int cy)
{
	camera_frustrum(cur_cam, cx, cy);
}

void on_create()
{
	cur_cam = camera_create();

	struct Vertex {
		V3 pos;
		V3 nor;
	};

	Vertex vertices[chunk_height * chunk_width];
	for (int j = 0; j < chunk_height; j++) {
		for (int i = 0; i < chunk_width; i++) {
			unsigned index = j * chunk_width + i;
			vertices[index].pos.x = i;
			vertices[index].pos.y = (rand() % 1000) / 1000.f;
			vertices[index].pos.z = j;
			vertices[index].nor = {};
		}
	}

	struct QuadIndices {
		unsigned i[6];
	};

	const int row_quads = chunk_height - 1;
	const int col_quads = chunk_width - 1;
	QuadIndices indices[row_quads * col_quads];
	for (int j = 0; j < row_quads; j++) {
		for (int i = 0; i < col_quads; i++) {
			unsigned pos = j * col_quads + i;
			indices[pos].i[0] = j * chunk_width + i;
			indices[pos].i[1] = j * chunk_width + i + 1;
			indices[pos].i[2] = (j + 1) * chunk_width + i;
			
			indices[pos].i[3] = j * chunk_width + i + 1;
			indices[pos].i[4] = (j + 1) * chunk_width + i + 1;
			indices[pos].i[5] = (j + 1) * chunk_width + i;
		}
	}

	for (int j = 0; j < row_quads; j++) {
		for (int i = 0; i < col_quads; i++) {
			unsigned pos = j * col_quads + i;
			for (int tri = 0; tri < 2; tri++) {
				Vertex *a = &vertices[indices[pos].i[tri * 3 + 0]];
				Vertex *b = &vertices[indices[pos].i[tri * 3 + 1]];
				Vertex *c = &vertices[indices[pos].i[tri * 3 + 2]];
				V3 cp = Matrix::cross(b->pos - a->pos, c->pos - a->pos);
				cp = cp * -1.f;
				a->nor += cp;
				b->nor += cp;
				c->nor += cp;
			}
		}
	}

	for (int j = 0; j < chunk_height; j++) {
		for (int i = 0; i < chunk_width; i++) {
			unsigned index = j * chunk_width + i;
			vertices[index].nor = Matrix::normalise(vertices[index].nor);
		}
	}

	unsigned default_vertex_shader;
	unsigned default_fragment_shader;
	unsigned lighting_fragment_shader;

	shader_program = glCreateProgram();
	lighting_shader_program = glCreateProgram();
	default_vertex_shader = gl_load_shader_from_file("default_vertex_shader.gl", shader_program, GL_VERTEX_SHADER);
	default_fragment_shader = gl_load_shader_from_file("default_fragment_shader.gl", shader_program, GL_FRAGMENT_SHADER);
	glAttachShader(shader_program, default_vertex_shader);
	glAttachShader(shader_program, default_fragment_shader);
	glLinkProgram(shader_program);
    glUseProgram(shader_program);
    if (!gl_check_program_link_log(shader_program)) {
		MessageBoxA(0, "Something went wrong during shader program linking!", "Fatal Error", 0);
	}

	unsigned a_pos = glGetAttribLocation(shader_program, "a_pos");
	unsigned a_nor = glGetAttribLocation(shader_program, "a_nor");
	unsigned a_lighting_pos = glGetAttribLocation(lighting_shader_program, "a_pos");

	transform_loc = glGetUniformLocation(shader_program, "transform");
	object_colour_loc = glGetUniformLocation(shader_program, "object_colour");
	model_loc = glGetUniformLocation(shader_program, "model");
	light_pos_loc = glGetUniformLocation(shader_program, "light_pos");

	glUseProgram(0);

	glAttachShader(lighting_shader_program, default_vertex_shader);
	glAttachShader(lighting_shader_program, default_fragment_shader);
	glLinkProgram(lighting_shader_program);
    glUseProgram(lighting_shader_program);
    if (!gl_check_program_link_log(shader_program)) {
		MessageBoxA(0, "Something went wrong during shader program linking!", "Fatal Error", 0);
	}

	glDeleteShader(default_vertex_shader);
	glDeleteShader(default_fragment_shader);
	glDeleteShader(lighting_fragment_shader);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, chunk_height * chunk_width * 6 * sizeof(float), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, row_quads * col_quads * 6 * sizeof(unsigned), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(a_pos, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(a_pos);

	glVertexAttribPointer(a_nor, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(a_nor);
}

void on_close()
{
	camera_destroy(cur_cam);

	glBindVertexArray(0);

	glDeleteBuffers(1, &ebo);
	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shader_program);
    glDeleteProgram(lighting_shader_program);
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
	srand(time(NULL));
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

	float light_pos[3] = { 0.f, 5.f, 0.f };

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

			float terrain_colour[3] = { 0.0f, 1.0f, 0.0 };
			glUniform3fv(object_colour_loc, 1, terrain_colour);
			glUniform3fv(light_pos_loc, 1, light_pos);
			
			glClearColor(0.2f, 0.2f, 0.3f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			for (int j = 0; j < 1; j++) {
				for (int i = 0; i < 1; i++) {
					float model[16];
					Matrix::identity(model);
					Matrix::translate(model, i * (chunk_width - 1), 0.f, j * (chunk_height -1));
					Matrix::scale(model, 1.f, 1.0f, 1.f);

					glUniformMatrix4fv(model_loc, 1, GL_FALSE, model);

					float mv[16], mvp[16];
					Matrix::identity(mv);
					Matrix::identity(mvp);
					Matrix::multiply(mv, cur_cam->view, model);
					Matrix::multiply(mvp, cur_cam->frustrum, mv);
					glUniformMatrix4fv(transform_loc, 1, GL_FALSE, mvp);

					glDrawElements(GL_TRIANGLES, (chunk_width - 1) * (chunk_height - 1) * 6, GL_UNSIGNED_INT, NULL);
				}
			}

			SwapBuffers(wglGetCurrentDC());
		
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