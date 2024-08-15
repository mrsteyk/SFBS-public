#pragma once

using NVSDK_NGX_Parameter_GetI_fn = uintptr_t(__fastcall*)(void* s, const char* name, int* out);
using NVSDK_NGX_Parameter_GetUI_fn = uintptr_t(__fastcall*)(void* s, const char* name, unsigned int* out);

using NVSDK_NGX_Parameter_SetI_fn = uintptr_t(__fastcall*)(void* s, const char* name, int in);
using NVSDK_NGX_Parameter_SetUI_fn = uintptr_t(__fastcall*)(void* s, const char* name, unsigned int in);

using NVSDK_NGX_D3D12_Init_fn = uintptr_t(__fastcall*)(uintptr_t id, const wchar_t* p, ID3D12Device* device, int ver);
using NVSDK_NGX_D3D12_Init_Ext_fn = uintptr_t(__fastcall*)(uintptr_t id, const wchar_t* p, ID3D12Device* device, int ver, void* ext);
// Struct is opaque to the app
using NVSDK_NGX_D3D12_GetCapabilityParameters_fn = uintptr_t(__fastcall*)(void** s);

// Brih
using NVSDK_NGX_Parameter_SetD3d12Resource_fn = uintptr_t(__fastcall*)(void* s, const char* name, void* in);
using NVSDK_NGX_Parameter_SetF_fn = uintptr_t(__fastcall*)(void* s, const char* name, float in);

using NVSDK_NGX_D3D12_EvaluateFeature_fn = uintptr_t(__fastcall*)(void* cmd_list, void* handle, void* opaque, void*);

extern HMODULE nvngx_mod;
extern void* g_Opaque;

extern bool g_bDLSS;
extern bool g_bDLSSG;

void InitDLSS(void* device_);
void UninitDLSS();

void* CreateDLSSFeature(void* device_, uint32_t* maxRenderSize, uint32_t* displaySize, int preset);
void ReleaseDLSSFeature(void* ctx);

struct STKDLSSCtx {
	void* dlss_handle;
};
extern void* g_DLSS_handle;
#ifdef DLSSG
extern void* g_DLSSG_handle;
#endif