#include "win32-opengl.h"
#include "opengl-util.h"
#include "app.h"

static bool window_resized;
static bool running;
static HGLRC glrc;

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

	app_memory memory = {};
	memory.permenant_storage_size = Megabytes(64);
	memory.permenant_storage = VirtualAlloc(0, memory.permenant_storage_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	
	if (memory.permenant_storage) {
		real64 dt = 0;
		running = true;

		while (running) {
			MSG msg;

			LARGE_INTEGER start, freq;
			QueryPerformanceCounter(&start);
			QueryPerformanceFrequency(&freq);

			app_input input = {};
			app_window_info window_info = {};
			window_resized = false;

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