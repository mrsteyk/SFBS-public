#include <iostream>

#include "core.hh"

#include <MinHook.h>
#include <spdlog/spdlog.h>

#include "renderpass_hook.hh"
#include "d3d12_hook.hh"

uintptr_t g_Starfield = 0;
HMODULE g_DXGI = nullptr;
void* g_CreateDXGIFactory2 = nullptr;
void* g_pDXGIFactory2 = nullptr;

inline HMODULE getDXGI() {
	if (g_DXGI) return g_DXGI;

	wchar_t infoBuf[4096];
	GetSystemDirectory(infoBuf, 4096);
	lstrcatW(infoBuf, L"\\dxgi.dll");

	g_DXGI = LoadLibraryW(infoBuf);

	g_CreateDXGIFactory2 = GetProcAddress(g_DXGI, "CreateDXGIFactory2");

	return g_DXGI;
}

BOOL DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);

#ifndef NDEBUG
		// Alloc console on debug builds
		AllocConsole();
		FILE* file = nullptr;
		freopen_s(&file, "CONIN$", "r", stdin);
		freopen_s(&file, "CONOUT$", "w", stdout);
		freopen_s(&file, "CONOUT$", "w", stderr);

		// Set logger level to trace
		spdlog::set_level(spdlog::level::trace);
#endif

		// Constructors
		g_Starfield = uintptr_t(GetModuleHandleA(nullptr));
		getDXGI();
		if (auto ret = MH_Initialize()) {
			// std::fprintf(stderr, "MH_Initialize() = %p\n", PVOID(ret));
			spdlog::error("MH_Initialize() = 0x{X}", (unsigned int)ret);
			// Cry?
			return FALSE;
		}

		// Create required hooks...
		// MH_CreateHook();

		HookFSR2UpscaleRenderPass();
		
		// Needed to hook queue creation as soon as possible!
		HookDX12();
	}
	else if (fdwReason == DLL_PROCESS_DETACH) {
		// Destructors
		UnHookFSR2UpscaleRenderPass();
		// UninitDLSS();

		MH_Uninitialize();
	}
	
	return TRUE;
}


namespace proxy {

	// nvgnx wants this or something
	extern "C" __declspec(dllexport) HRESULT CreateDXGIFactory1(REFIID riid, void** ppFactory) {
		auto orig = reinterpret_cast<decltype(&proxy::CreateDXGIFactory1)>(GetProcAddress(g_DXGI, "CreateDXGIFactory1"));
		return orig(riid, ppFactory);
	}

	// FUCKING NVAPI64 NEEDS THIS TO CHECK SUPPORT FOR DIFFERENT SHIT
	extern "C" __declspec(dllexport) HRESULT CreateDXGIFactory(REFIID riid, void** ppFactory) {
		auto orig = reinterpret_cast<decltype(&proxy::CreateDXGIFactory)>(GetProcAddress(g_DXGI, "CreateDXGIFactory"));
		return orig(riid, ppFactory);
	}

	extern "C" __declspec(dllexport) HRESULT CreateDXGIFactory2(UINT Flags, const IID* const riid, void** ppFactory) {
		auto orig = reinterpret_cast<decltype(&proxy::CreateDXGIFactory2)>(g_CreateDXGIFactory2);

		auto ret = orig(Flags, riid, ppFactory);

		// idk?
		g_pDXGIFactory2 = *ppFactory;
		HookDXGIFactory2(g_pDXGIFactory2);

		return ret;
	}
}