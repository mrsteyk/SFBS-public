#include "core.hh"

#include <d3d12.h>
#include <dxgi1_4.h>

#include "MinHook.h"
#include "memory.hh"

// Real SDK is under NVIDIA RTX SDK license
// RE ftw

// Very popular one
constexpr uintptr_t MEME_ID = 231313132;
// Version check constants
constexpr const char* NVSDK_C_NeedsUpdatedDriver = "SuperSampling.NeedsUpdatedDriver";
constexpr const char* NVSDK_C_MinDriverVersionMajor = "SuperSampling.MinDriverVersionMajor";
constexpr const char* NVSDK_C_MinDriverVersionMinor = "SuperSampling.MinDriverVersionMinor";
constexpr const char* NVSDK_C_Available = "SuperSampling.Available";
constexpr const char* NVSDK_C_FeatureInitResult = "SuperSampling.FeatureInitResult";

constexpr const char* NVSDK_FG_C_NeedsUpdatedDriver = "FrameGeneration.NeedsUpdatedDriver";
constexpr const char* NVSDK_FG_C_MinDriverVersionMajor = "FrameGeneration.MinDriverVersionMajor";
constexpr const char* NVSDK_FG_C_MinDriverVersionMinor = "FrameGeneration.MinDriverVersionMinor";
constexpr const char* NVSDK_FG_C_Available = "FrameGeneration.Available";
constexpr const char* NVSDK_FG_C_FeatureInitResult = "FrameGeneration.FeatureInitResult";

#include "dlss.hh"

HMODULE nvngx_mod = nullptr;
//HMODULE dlss_mod = nullptr;
//HMODULE dlssg_mod = nullptr;

bool g_bDLSS = false;
bool g_bDLSSG = false;

inline HMODULE try_loading_nvngx(const char* path) {
	auto ps = std::string(path);

	auto ret = LoadLibraryA((ps + "\\_nvngx.dll").c_str());
	if (ret)
		return ret;
	ret = LoadLibraryA((ps + "\\nvngx.dll").c_str());
	return ret;
}

HMODULE get_nvngx_mod() {
	if (nvngx_mod)
		return nvngx_mod;

	// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\nvlddmkm\NGXCore
	// HKEY_LOCAL_MACHINE\SOFTWARE\NVIDIA Corporation\Global\NGXCore
	HKEY key = 0;
	auto ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", 0, KEY_READ, &key);
	if (!ret) {
		// succ open
		char path[1024];
		DWORD size = sizeof(path);
		ret = RegQueryValueExA(key, "NGXPath", nullptr, nullptr, (LPBYTE)&path, &size);
		if (!ret) {
			// succ query
			spdlog::debug(__FUNCTION__ ": trying {}", std::string_view(path));
			nvngx_mod = try_loading_nvngx(path);
			if (nvngx_mod)
				return nvngx_mod;
		}
		RegCloseKey(key);
	}

	ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", 0, KEY_READ, &key);
	if (!ret) {
		// succ open
		char path[1024];
		DWORD size = sizeof(path);
		ret = RegQueryValueExA(key, "FullPath", nullptr, nullptr, (LPBYTE)&path, &size);
		if (!ret) {
			// succ query
			spdlog::debug(__FUNCTION__ ": trying {}", std::string_view(path));
			nvngx_mod = try_loading_nvngx(path);
			if (nvngx_mod)
				return nvngx_mod;
		}
		RegCloseKey(key);
	}

	return nullptr;
}

#ifdef DLSSG
// OMITTED FOR MY OWN SAFETY
#endif

void* g_Opaque = nullptr;
void InitDLSS(void* device_) {
	auto device = (ID3D12Device*)device_;
	//dlss_mod = LoadLibraryA("nvngx_dlss.dll");
	nvngx_mod = get_nvngx_mod();
	//spdlog::debug("dlss_mod = {}", PVOID(dlss_mod));
	spdlog::debug("nvngx_mod = {}", PVOID(nvngx_mod));
	
	//if (!dlss_mod || !nvngx_mod) {
	if (!nvngx_mod) {
		return;
	}

	// Before initialising DLSS we need to patch nvngx
#ifdef DLSSG
	{
		// OMITTED FOR MY OWN SAFETY
	}
#endif

	auto NVSDK_NGX_D3D12_Init = NVSDK_NGX_D3D12_Init_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_Init"));
	auto NVSDK_NGX_D3D12_Init_Ext = NVSDK_NGX_D3D12_Init_Ext_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_Init_Ext"));
	// DLSS needs at least 21
	//*
	auto ret = NVSDK_NGX_D3D12_Init(MEME_ID, L".", device, 21);
	spdlog::debug("NVSDK_NGX_D3D12_Init = {}", PVOID(ret));
	//*/

	if (ret != 1) {
		return;
	}

	void* opaque;
	auto NVSDK_NGX_D3D12_GetCapabilityParameters = NVSDK_NGX_D3D12_GetCapabilityParameters_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_GetCapabilityParameters"));
	ret = NVSDK_NGX_D3D12_GetCapabilityParameters(&opaque);
	spdlog::debug("NVSDK_NGX_D3D12_GetCapabilityParameters = {}", PVOID(ret));
	g_Opaque = opaque;

	// Check drivers
	// Failsafe to "force" update state
	int update = 1;
	unsigned int mj = 0, mn = 0;

	auto vtbl = *(uintptr_t**)opaque;
	auto NVSDK_NGX_Parameter_GetI = NVSDK_NGX_Parameter_GetI_fn(vtbl[11]);
	auto NVSDK_NGX_Parameter_GetUI = NVSDK_NGX_Parameter_GetUI_fn(vtbl[12]);
	
	ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_C_NeedsUpdatedDriver, &update);
#ifndef NDEBUG
	spdlog::debug("NVSDK_C_NeedsUpdatedDriver = {} | {}", PVOID(ret), update);
#endif
	ret = NVSDK_NGX_Parameter_GetUI(opaque, NVSDK_C_MinDriverVersionMajor, &mj);
#ifndef NDEBUG
	spdlog::debug("NVSDK_C_MinDriverVersionMajor = {} | {}", PVOID(ret), mj);
#endif
	ret = NVSDK_NGX_Parameter_GetUI(opaque, NVSDK_C_MinDriverVersionMinor, &mn);
#ifndef NDEBUG
	spdlog::debug("NVSDK_C_MinDriverVersionMinor = {} | {}", PVOID(ret), mn);
#endif

	if (update) {
		spdlog::error("Failed to initialise DLSS module! You need to update your driver at least to {}.{} and have NVidia RTX card", mj, mn);
		return;
	}
	
	// Are we gucci?
	int avail = 0;
	ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_C_Available, &avail);
#ifndef NDEBUG
	spdlog::debug("NVSDK_C_Available = {} | {}", PVOID(ret), avail);
#endif

	if (!avail) {
		spdlog::error("DLSS reported as being unavaible!");
		// Try to get some info on that matter...
		int res = 0xBAD00000;
		ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_C_FeatureInitResult, &res);
		spdlog::debug("NVSDK_C_FeatureInitResult = {} | {}", PVOID(ret), res);
		return;
	}

	// Noice
	spdlog::info("DLSS is available!");
	g_bDLSS = true;

	//InitDLSSG(opaque);

#ifdef DLSSG
	// Do the DLSSG
	{
		ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_FG_C_NeedsUpdatedDriver, &update);
#ifndef NDEBUG
		spdlog::debug("NVSDK_FG_C_NeedsUpdatedDriver = {} | {}", PVOID(ret), update);
#endif
		ret = NVSDK_NGX_Parameter_GetUI(opaque, NVSDK_FG_C_MinDriverVersionMajor, &mj);
#ifndef NDEBUG
		spdlog::debug("NVSDK_FG_C_MinDriverVersionMajor = {} | {}", PVOID(ret), mj);
#endif
		ret = NVSDK_NGX_Parameter_GetUI(opaque, NVSDK_FG_C_MinDriverVersionMinor, &mn);
#ifndef NDEBUG
		spdlog::debug("NVSDK_FG_C_MinDriverVersionMinor = {} | {}", PVOID(ret), mn);
#endif

		if (update) {
			spdlog::error("Failed to initialise DLSSG module! You need to update your driver at least to {}.{} and have NVidia RTX 40+ card", mj, mn);
			return;
		}

		// Are we gucci?
		int avail = 0;
		ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_FG_C_Available, &avail);
#ifndef NDEBUG
		spdlog::debug("NVSDK_FG_C_Available = {} | {}", PVOID(ret), avail);
#endif

		if (!avail) {
			spdlog::error("DLSSG reported as being unavaible!");
			// Try to get some info on that matter...
			unsigned int res = 0xBAD00000;
			ret = NVSDK_NGX_Parameter_GetI(opaque, NVSDK_FG_C_FeatureInitResult, (int*)&res);
			spdlog::debug("NVSDK_FG_C_FeatureInitResult = {} | {}", PVOID(ret), PVOID(res));
			return;
		}

		// Rich fuck
		spdlog::info("DLSSG is available!");
		g_bDLSSG = true;
	}
#endif
}

void UninitDLSS() {
	// TODO: free stuff here
	spdlog::info(__FUNCTION__ " success!");
}

void InitDLSSG(void* opaque) {
	//dlssg_mod = LoadLibraryA("nvngx_dlssg.dll");
	//spdlog::debug("dlssg_mod = {}", PVOID(dlssg_mod));
}

// Let's put all the more public stuff at the butto
void* g_DLSS_handle = nullptr;
#ifdef DLSSG
void* g_DLSSG_handle = nullptr;
#endif
void* CreateDLSSFeature(void* device_, uint32_t* maxRenderSize, uint32_t* displaySize, int preset) {
	// SS is 1
	// FG is 11
	using NVSDK_NGX_D3D12_CreateFeature_fn = uintptr_t(__fastcall*)(void* cmdlist, uintptr_t id, void* opaque, void** out_handle);
	auto NVSDK_NGX_D3D12_CreateFeature = NVSDK_NGX_D3D12_CreateFeature_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_CreateFeature"));



	auto device = (ID3D12Device*)device_;

	ID3D12Fence* fence = nullptr;
	auto hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
#ifndef NDEBUG
	spdlog::debug("device->CreateFence = {:X}", hr);
#endif

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.NodeMask = 1;

	ID3D12CommandQueue* cmdQueue = nullptr;
	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
	spdlog::debug("device->CreateCommandQueue = {:X}", hr);

	ID3D12CommandAllocator* cmdAlloc = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
#ifndef NDEBUG
	spdlog::debug("device->CreateCommandAllocator = {:X}", hr);
#endif

	ID3D12GraphicsCommandList* command_list = nullptr;
	// I am a new culprit
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&command_list));
#ifndef NDEBUG
	spdlog::debug("device->CreateCommandList = {:X}", hr);
#endif

	hr = command_list->Reset(cmdAlloc, nullptr);
	// command_list->ResourceBarrier(1, &Barrier);
#ifndef NDEBUG
	spdlog::debug("command_list->Reset = {:X}", hr);
#endif

	auto ovtbl = *(uintptr_t**)g_Opaque;
	auto NVSDK_NGX_Parameter_SetI = NVSDK_NGX_Parameter_SetI_fn(ovtbl[3]);
	auto NVSDK_NGX_Parameter_SetUI = NVSDK_NGX_Parameter_SetUI_fn(ovtbl[4]);

	// Set dlss shit
	NVSDK_NGX_Parameter_SetUI(g_Opaque, "Width", maxRenderSize[0]);
	NVSDK_NGX_Parameter_SetUI(g_Opaque, "Height", maxRenderSize[1]);
	NVSDK_NGX_Parameter_SetUI(g_Opaque, "OutWidth", displaySize[0]);
	NVSDK_NGX_Parameter_SetUI(g_Opaque, "OutHeight", displaySize[1]);
	NVSDK_NGX_Parameter_SetI(g_Opaque, "PerfQualityValue", 0);
	NVSDK_NGX_Parameter_SetI(g_Opaque, "RTXValue", 0);
	NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSS.Feature.Create.Flags", 1 | 2 | 8 | 64);
	//NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSS.Feature.Create.Flags", 2 | 8 | 64);
	NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSS.Enable.Output.Subrects", 0);

	// Default is 0 - default?
	if (preset == 0) {
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.DLAA", 0x80000006);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Quality", 0x80000004);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Balanced", 0x80000004);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Performance", 0x80000004);
		//NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Performance", 1);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.UltraPerformance", 0x80000006);
	}
	else {
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.DLAA", preset);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Quality", preset);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Balanced", preset);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.Performance", preset);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSS.Hint.Render.Preset.UltraPerformance", preset);
	}
	
	// Exec dlss shit
	void* dlss_handle;
	auto ret = NVSDK_NGX_D3D12_CreateFeature(command_list, 1, g_Opaque, &dlss_handle);
	spdlog::debug("NVSDK_NGX_D3D12_CreateFeature(DLSS) = {} | {}", PVOID(ret), dlss_handle);

	hr = command_list->Close();

	cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&command_list);
	hr = cmdQueue->Signal(fence, 1);
#ifndef NDEBUG
	spdlog::debug("cmdQueue->Signal = {:X}", hr);
#endif
	
	auto ctx = new STKDLSSCtx();
	ctx->dlss_handle = dlss_handle;
	g_DLSS_handle = dlss_handle;

	if (ret == 1) {
#ifdef DLSSG
		// DLSS was made, now I can try to make DLSSG...
		spdlog::info("DLSS was made successfully, now trying to make DLSSG..");

		ID3D12Fence* fence = nullptr;
		auto hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		spdlog::debug("device->CreateFence = {:X}", hr);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 1;

		ID3D12CommandQueue* cmdQueue = nullptr;
		hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&cmdQueue));
		spdlog::debug("device->CreateCommandQueue = {:X}", hr);

		ID3D12CommandAllocator* cmdAlloc = nullptr;
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc));
		spdlog::debug("device->CreateCommandAllocator = {:X}", hr);

		ID3D12GraphicsCommandList* command_list = nullptr;
		// I am a new culprit
		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc, nullptr, IID_PPV_ARGS(&command_list));
		spdlog::debug("device->CreateCommandList = {:X}", hr);
	
		// command_list->ResourceBarrier(1, &Barrier);
		spdlog::debug("command_list->Reset = {:X}", hr);

		// Set opaque shit?
		// DLSSG.BackbufferFormat - UI
		extern IDXGISwapChain3* g_pSwapChain;
		ID3D12Resource* pBackBuffer = nullptr;
		g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		auto format = (unsigned int)pBackBuffer->GetDesc().Format;
		spdlog::debug("DLSSG.Backbufferformat = {}/{:X}", format, format);
		NVSDK_NGX_Parameter_SetUI(g_Opaque, "DLSSG.BackbufferFormat", format);

		// Exec..
		void* dlssg_handle;
		ret = NVSDK_NGX_D3D12_CreateFeature(command_list, 11, g_Opaque, &dlssg_handle);
		spdlog::debug("NVSDK_NGX_D3D12_CreateFeature(DSLLG) = {} | {}", PVOID(ret), dlssg_handle);
		
		hr = command_list->Close();

		cmdQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&command_list);
		hr = cmdQueue->Signal(fence, 2);
		spdlog::debug("cmdQueue->Signal = {:X}", hr);

		command_list->Release();
		cmdAlloc->Release();
		cmdQueue->Release();
		fence->Release();

		if (ret == 1) {
			spdlog::info("DLSSG Initialised, rich fuck.");
			g_DLSSG_handle = dlssg_handle;
		}
		else {
			spdlog::error("DLSSG isn't initialised");
		}
#endif
	}
	else {
		spdlog::error("DLSS init fail!");
	}

	command_list->Release();
	cmdAlloc->Release();
	cmdQueue->Release();
	fence->Release();

	return ctx;
}
void ReleaseDLSSFeature(void* ctx_) {
	if (!ctx_)
		return;
	auto ctx = (STKDLSSCtx*)ctx_;

	using NVSDK_NGX_D3D12_ReleaseFeature_fn = uintptr_t(__fastcall*)(void* handle);
	auto NVSDK_NGX_D3D12_ReleaseFeature = NVSDK_NGX_D3D12_ReleaseFeature_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_ReleaseFeature"));
	auto ret = NVSDK_NGX_D3D12_ReleaseFeature(ctx->dlss_handle);
#ifndef NDEBUG
	spdlog::debug("NVSDK_NGX_D3D12_ReleaseFeature(DLSS) = {}", PVOID(ret));
#endif
#ifdef DLSSG
	ret = NVSDK_NGX_D3D12_ReleaseFeature(g_DLSSG_handle);
#ifndef NDEBUG
	spdlog::debug("NVSDK_NGX_D3D12_ReleaseFeature(DLSSG) = {}", PVOID(ret));
#endif
	g_DLSSG_handle = nullptr;
#endif

	delete ctx;
}
