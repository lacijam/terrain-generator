#include "win32-opengl.h"
#include "opengl-util.h"
#include "app.h"

#include "imgui-master\imgui.h"
#include "imgui-master\imgui_impl_win32.h"
#include "imgui-master\imgui_impl_opengl3.h"
#include "imgui-master\imgui_internal.h"

static bool window_resized;
static bool running;
static HGLRC glrc;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
    	return true;

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
	AllocConsole();
	freopen("CONOUT$", "w", stdout);

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
	UpdateWindow(hwnd);

	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui_ImplWin32_Init(hwnd);
            
    //Init OpenGL Imgui Implementation
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);

	// Setup style
    ImGui::StyleColorsClassic();

	app_memory memory = {};
	memory.permenant_storage_size = Megabytes(8000);
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
			
			BYTE keys[256];
			GetKeyboardState(keys);
			input.keyboard.wireframe.started_down = keys['G'] & 0x80;
			input.keyboard.reset.started_down = keys['R'] & 0x80;
			input.keyboard.fly.started_down = keys['F'] & 0x80;

			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					running = false;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			GetKeyboardState(keys);
			input.keyboard.forward.ended_down = keys['W'] & 0x80;
			input.keyboard.backward.ended_down = keys['S'] & 0x80;
			input.keyboard.left.ended_down = keys['A'] & 0x80;
			input.keyboard.right.ended_down = keys['D'] & 0x80;
			input.keyboard.cam_up.ended_down = keys[VK_UP] & 0x80;
			input.keyboard.cam_down.ended_down = keys[VK_DOWN] & 0x80;
			input.keyboard.cam_left.ended_down = keys[VK_LEFT] & 0x80;
			input.keyboard.cam_right.ended_down = keys[VK_RIGHT] & 0x80;
			input.keyboard.wireframe.ended_down = keys['G'] & 0x80;
			input.keyboard.wireframe.toggled = !input.keyboard.wireframe.started_down && keys['G'] & 0x80;
			input.keyboard.reset.ended_down = keys['R'] & 0x80;
			input.keyboard.reset.toggled = !input.keyboard.reset.started_down && keys['R'] & 0x80;
			input.keyboard.fly.ended_down = keys['F'] & 0x80;
			input.keyboard.fly.toggled = !input.keyboard.fly.started_down && keys['F'] & 0x80;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplWin32_NewFrame();

			RECT client;
			GetClientRect(hwnd, &client);
			window_info.w = client.right;
			window_info.h = client.bottom;
			window_info.resize = window_resized;
			window_info.running = running;
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

	ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    ImGui_ImplWin32_Shutdown();

    wglMakeCurrent(0, 0);
    wglDeleteContext(glrc);

    return 0;
}