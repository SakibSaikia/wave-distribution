#include <demo.h>

#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyDemo();
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE)
		{
			EnableWindow(hWnd, TRUE);
		}
		else if (LOWORD(wParam) == WA_INACTIVE)
		{
			EnableWindow(hWnd, FALSE);
		}
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool InitializeWindow(HINSTANCE instanceHandle, HWND& windowHandle, const uint32_t resX, const uint32_t resY)
{
	WNDCLASS desc;
	desc.style = CS_HREDRAW | CS_VREDRAW;
	desc.lpfnWndProc = WndProc;
	desc.cbClsExtra = 0;
	desc.cbWndExtra = 0;
	desc.hInstance = instanceHandle;
	desc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	desc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	desc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
	desc.lpszMenuName = nullptr;
	desc.lpszClassName = L"demo_window";

	if (!RegisterClass(&desc))
	{
		MessageBox(nullptr, TEXT("Failed to register window"), nullptr, 0);
		return false;
	}

	windowHandle = CreateWindowEx(
		WS_EX_APPWINDOW,
		L"demo_window",
		TEXT("wave-distribution-demo"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		resX,
		resY,
		nullptr,
		nullptr,
		instanceHandle,
		nullptr
	);

	if (!windowHandle)
	{
		MessageBox(nullptr, TEXT("Failed to create window"), nullptr, 0);
		return false;
	}
	else
	{
		ShowWindow(windowHandle, SW_SHOW);
		UpdateWindow(windowHandle);
		return true;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
	const uint32_t windowWidth = 1280;
	const uint32_t windowHeight = 720;

	MSG msg = {};
	HWND windowHandle = {};

	InitializeWindow(hInstance, windowHandle, windowWidth, windowHeight);
	InitializeDemo(windowHandle, windowWidth, windowHeight);
	RenderDemo();

	while (msg.wParam != VK_ESCAPE)
	{
		if (!windowHandle)
		{
			continue;
		}

		if (PeekMessage(&msg, windowHandle, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	DestroyWindow(windowHandle);

	return static_cast<int>(msg.wParam);
}
