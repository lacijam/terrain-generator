#include "win32-opengl.h"
#include "opengl-util.h"
#include "app.h"

#include "imgui-master\imgui.h"
#include "imgui-master\imgui_impl_win32.h"
#include "imgui-master\imgui_impl_opengl3.h"
#include "imgui-master\imgui_internal.h"

static bool window_resized;
static bool running;
static bool active;
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
		active = wParam != SIZE_MINIMIZED;
		window_resized = true;
		break;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return TRUE;
}

real64 GetHighResolutionTime(LARGE_INTEGER freq)
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	time.QuadPart *= 1000000;
	time.QuadPart /= freq.QuadPart;
	return time.QuadPart / 1000.;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#ifdef _DEBUG
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
#endif

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

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
		, 1366, 768, NULL, NULL, GetModuleHandle(NULL), 0
	);

	if (!hwnd) {
		MessageBoxA(0, "Failed to create window", "Fatal Error", 0);
		return 1;
	}

	win32_init_opengl_extensions();
	glrc = win32_create_gl_context(hwnd);

	ShowWindow(hwnd, nShowCmd);

	RECT rc;
	GetWindowRect(hwnd, &rc);
	int xPos = (GetSystemMetrics(SM_CXSCREEN) - rc.right) / 2;
	int yPos = (GetSystemMetrics(SM_CYSCREEN) - rc.bottom) / 2;

	SetWindowPos(hwnd, 0, xPos, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
	
	UpdateWindow(hwnd);

	CreateDirectory("./export", NULL);

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

	RECT client;
	GetClientRect(hwnd, &client);
	u32 w = client.right;
	u32 h = client.bottom;
	app_state *state = app_init(w, h);

	if (state) {
		real64 dt = 0;
		real64 dt_elapsed = 0;
		running = true;

		const u32 FPS = 60;
		const real32 ms_per_frame = 1000. / FPS;

		while (running) {
			MSG msg;

			real64 start;
			start = GetHighResolutionTime(freq);

			app_input input = {};
			app_window_info window_info = {};
			window_resized = false;

			BYTE keys[256];
			GetKeyboardState(keys);
			input.keyboard.wireframe.started_down = keys['G'] & 0x80;
			input.keyboard.fly.started_down = keys['F'] & 0x80;

			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					running = false;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (active) {
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
				input.keyboard.fly.ended_down = keys['F'] & 0x80;
				input.keyboard.fly.toggled = !input.keyboard.fly.started_down && keys['F'] & 0x80;

				RECT client;
				GetClientRect(hwnd, &client);
				window_info.w = client.right;
				window_info.h = client.bottom;
				window_info.resize = window_resized;
				window_info.running = running;

				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplWin32_NewFrame();

				app_update_and_render(ms_per_frame / 1000.f, state, &input, &window_info);

				real64 finish = GetHighResolutionTime(freq);
				dt = finish - start;

				if (dt < ms_per_frame) {
					while (GetHighResolutionTime(freq) - start < ms_per_frame);
					SwapBuffers(wglGetCurrentDC());
				}
			}
			else {
				Sleep(10);
			}
		}

		delete state;
	}

	ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
    ImGui_ImplWin32_Shutdown();

    wglMakeCurrent(0, 0);
    wglDeleteContext(glrc);

    return 0;
}