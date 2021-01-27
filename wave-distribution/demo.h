#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WindowsX.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <memory>
#include <winrt/base.h>

bool WINAPI InitializeDemo(const HWND& windowHandle, const uint32_t resX, const uint32_t resY);
void WINAPI RenderDemo();
void WINAPI DestroyDemo();
