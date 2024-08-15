#include <cstdint>

#include <intrin.h>

#include "core.hh"

#include "renderpass_hook.hh"

#include "MinHook.h"

#define INITGUID
#include <d3d12.h>

#include "dlss.hh"

struct {
	// bool, &1 = 0/1
	uintptr_t(__fastcall* dtor)(uintptr_t thiz, char a2);

	uintptr_t(__fastcall* vfunc1)(uintptr_t thiz, uint32_t* a2,
		uintptr_t a3,
		__int64* a4,
		char a5,
		char a6,
		unsigned __int16 a7);

	uintptr_t(__fastcall* vfunc2)(uintptr_t thiz, uintptr_t a2);

	// Make meat and potatoes
	void(__fastcall* vfunc3)(uintptr_t thiz, uintptr_t a2, uintptr_t a3);

	// thinks there's 1 arg only idk, doesn't hurt it's in a register?
	void(__fastcall* vfunc4)(uintptr_t thiz, uintptr_t a2);

	// noop
	uintptr_t(__fastcall* vfunc5)(uintptr_t thiz, uintptr_t a2, uintptr_t a3);
	// noop
	uintptr_t(__fastcall* vfunc6)(uintptr_t thiz, uintptr_t a2);

	// Devour meat and potatoes
	uintptr_t(__fastcall* Execute)(uintptr_t thiz, uintptr_t a2, uintptr_t a3);
} HookFSR2UpscaleRenderPass_vtbl;
static_assert(sizeof(HookFSR2UpscaleRenderPass_vtbl) == 8 * sizeof(void*), "Invalid vtbl size for HookFSR2UpscaleRenderPass");

#ifndef NDEBUG
uintptr_t dtor_hk(uintptr_t thiz, char a2) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::dtor({}, {}) from {}", PVOID(thiz), +a2, retaddr);

	return HookFSR2UpscaleRenderPass_vtbl.dtor(thiz, a2);
}

uintptr_t vfunc1_hk(uintptr_t thiz, uint32_t* a2,
	uintptr_t a3,
	__int64* a4,
	char a5,
	char a6,
	unsigned __int16 a7) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::vfunc1({}, {}, {}, {}, {}, {}, {}) from {}", PVOID(thiz), PVOID(a2), a3, PVOID(a4), +a5, +a6, a7, retaddr);

	return HookFSR2UpscaleRenderPass_vtbl.vfunc1(thiz, a2, a3, a4, a5, a6, a7);
}

uintptr_t vfunc2_hk(uintptr_t thiz, uintptr_t a2) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::vfunc2({}, {}) from {}", PVOID(thiz), PVOID(a2), retaddr);

	return HookFSR2UpscaleRenderPass_vtbl.vfunc2(thiz, a2);
}
#endif

// Check viewport sizes
// Recreate objects if required
void vfunc3_hk(uintptr_t thiz, uintptr_t a2, uintptr_t a3) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
#ifndef NDEBUG
	spdlog::trace("FSR2UpscaleRenderPass::vfunc3({}, {}, {}) from {}", PVOID(thiz), PVOID(a2), PVOID(a3), retaddr);
#endif

	// E8 ?? ?? ?? ?? 49 8B 48 38 BA 16 00 00 00
	using GetViewportInfoFromIdPtr_fn = void*(__fastcall*)(uint32_t* viewport);
	auto GetViewportInfoFromIdPtr = GetViewportInfoFromIdPtr_fn(g_Starfield + 0x33E4CA0);

	// those names I pulled from my ass idfk
	auto a2_current_viewport_id = *(unsigned int*)(a2 + 0x1C8);
	auto a2_viewport_array = (uintptr_t*)(a2 + 0x198);
	auto a2_current_viewport = a2_viewport_array[a2_current_viewport_id];
	uint32_t a2_current_viewport_id2 = *(uint32_t*)(a2_current_viewport + 32);

#ifndef NDEBUG
	spdlog::debug("vfunc3: a2 current viewport {} {} {}({})", a2_current_viewport_id, PVOID(a2_current_viewport), (a2_current_viewport_id2 & 0xFFFFFF), a2_current_viewport_id2>>24);
#endif

	// viewport_info gets changed in orig if needed
	// allocated 0x20
	// first 4 are sizes
	// @8 - flags
	// @16 - fsr context (0x10260)
	auto viewport_info = GetViewportInfoFromIdPtr(&a2_current_viewport_id2);
	auto viewport_info_sizes = *(uint16_t**)viewport_info;
	auto viewport_initialised = !!viewport_info_sizes;
	uint16_t viewport_info_sizes_dummy[4]{ 0 };
	if (!viewport_initialised)
		viewport_info_sizes = viewport_info_sizes_dummy;
#ifndef NDEBUG
	spdlog::debug("vfunc3: viewport_info {}@{}@{} {}x{} {}x{}", viewport_initialised, viewport_info, PVOID(viewport_info_sizes), viewport_info_sizes[0], viewport_info_sizes[1], viewport_info_sizes[2], viewport_info_sizes[3]);
#endif

	// 0F B7 C2 48 8D 15 ?? ?? ?? ?? 48 8B 04 C2 80
	using QueryReflection_u32_fn = uint32_t(__fastcall*)(uintptr_t query, uint16_t field_id);
	auto QueryReflection_u32 = QueryReflection_u32_fn(g_Starfield + 0x326DD4C);

	auto current_viewport_query = *(uintptr_t*)(a2_current_viewport + 0x38);

	uint32_t maxRenderSize[2] = { QueryReflection_u32(current_viewport_query, 0x16), QueryReflection_u32(current_viewport_query, 0x1d) };
	uint32_t displaySize[2] = { QueryReflection_u32(current_viewport_query, 0x10), QueryReflection_u32(current_viewport_query, 0x11) };
#ifndef NDEBUG
	spdlog::debug("vfunc3: current_viewport_query {} {}x{} {}x{}", PVOID(current_viewport_query), maxRenderSize[0], maxRenderSize[1], displaySize[0], displaySize[1]);
#endif

	//HookFSR2UpscaleRenderPass_vtbl.vfunc3(thiz, a2, a3);

	bool needs_recreation = false;
	needs_recreation = needs_recreation || !viewport_initialised;
	needs_recreation = needs_recreation || (maxRenderSize[0] > viewport_info_sizes[0]);
	needs_recreation = needs_recreation || (maxRenderSize[1] > viewport_info_sizes[1]);
	needs_recreation = needs_recreation || (displaySize[0] != viewport_info_sizes[2]);
	needs_recreation = needs_recreation || (displaySize[1] != viewport_info_sizes[3]);

	// Ecksde...
	extern int g_DLSS_preset;
	static int g_last_DLSS_preset = g_DLSS_preset;
	needs_recreation = needs_recreation || (g_last_DLSS_preset != g_DLSS_preset);

	if (needs_recreation) {
#ifndef NDEBUG
		spdlog::info("vfunc3: needs updating!");
#endif
		// *(_QWORD *)(qword_145912F80 + 0x28)
		auto deivce_stuff_outer = *(uintptr_t*)(g_Starfield + 0x5912F80);
		auto device_stuff = *(uintptr_t*)(deivce_stuff_outer + 0x28);
		/*
			maxRenderSize,
            displaySize,
            *(_QWORD *)(qword_145912F80 + 0x28),
            ViewportInfoFromIdPtr // aka viewport_info
		*/
		// Allocate a meme struct
		using pseudo_operator_new_fn = void* (__fastcall*)(size_t size);
		using pseudo_operator_delete_fn = void* (__fastcall*)(void* ptr);

		// E8 ?? ?? ?? ?? 4C 89 68 28
		auto pseudo_operator_new = pseudo_operator_new_fn(g_Starfield + 0x5AE9C8);
		// E8 ?? ?? ?? ?? 90 48 8D 7B 48
		auto pseudo_operator_delete = pseudo_operator_delete_fn(g_Starfield + 0x829130);

		auto new_s_ptr = pseudo_operator_new(0x20);
		ZeroMemory(new_s_ptr, 0x20);
		auto old_s_ptr = *(void**)viewport_info;
		*(void**)viewport_info = new_s_ptr;
		pseudo_operator_delete(old_s_ptr);

		viewport_info_sizes = *(uint16_t**)viewport_info;

		viewport_info_sizes[0] = maxRenderSize[0];
		viewport_info_sizes[1] = maxRenderSize[1];
		viewport_info_sizes[2] = displaySize[0];
		viewport_info_sizes[3] = displaySize[1];

		// set some flag idk
		*(uint32_t*)(*(uintptr_t*)viewport_info + 8) = 41;

		void* ctx = *(void**)(*(uintptr_t*)viewport_info + 16);
		if (ctx) {
			ReleaseDLSSFeature(ctx);
		}

		// No need to set context for a dummy example!
		// Now we are not so dumb huh...
		// Create DLSS shit
		extern ID3D12Device* g_pdevice;
		*(void**)(*(uintptr_t*)viewport_info + 16) = CreateDLSSFeature(g_pdevice, maxRenderSize, displaySize, g_DLSS_preset);
		g_last_DLSS_preset = g_DLSS_preset;
#ifndef NDEBUG
		spdlog::debug("vfunc3: New viewport_info ptr is {}", *(void**)viewport_info);
#endif
	}
}

#ifndef NDEBUG
void vfunc4_hk(uintptr_t thiz, uintptr_t a2) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::vfunc4({}, {}) from {}", PVOID(thiz), PVOID(a2), retaddr);

	HookFSR2UpscaleRenderPass_vtbl.vfunc4(thiz, a2);
}
#endif

#ifndef NDEBUG
// noop in orig
uintptr_t vfunc5_hk(uintptr_t thiz, uintptr_t a2, uintptr_t a3) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::vfunc5({}, {}, {}) from {}", PVOID(thiz), PVOID(a2), PVOID(a3), retaddr);

	return HookFSR2UpscaleRenderPass_vtbl.vfunc5(thiz, a2, a3);
}

// noop in orig
uintptr_t vfunc6_hk(uintptr_t thiz, uintptr_t a2) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
	spdlog::trace("FSR2UpscaleRenderPass::vfunc6({}, {}) from {}", PVOID(thiz), PVOID(a2), retaddr);

	return HookFSR2UpscaleRenderPass_vtbl.vfunc6(thiz, a2);
}
#endif

inline ID3D12Resource2* SlotGetCResource(uintptr_t slot) {
	if (!slot)
		return nullptr;

	return *(ID3D12Resource2**)(*(uintptr_t*)(slot + 0x48) + 0xA8);
}

#ifndef NDEBUG
inline std::wstring our_SlotGetName_ws(uintptr_t slot) {
	if (!slot)
		return L"";

	auto res = SlotGetCResource(slot);

	wchar_t name[1024] = {};
	UINT size = sizeof(name);
	res->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name);

	return name;
}

inline std::string our_SlotGetName(uintptr_t slot) {
	if (!slot)
		return "";

	auto ws = our_SlotGetName_ws(slot);

	// dumb conversion
	return std::string(ws.begin(), ws.end());
}
#endif

//bool in_fsr2 = false;
//void* objs_i_care[6];

/*
const char* strs[6] = {
	"InputColor",
	"InputDepth",
	"InputMotionVectors",
	"InputReactiveMap",
	"TransparencyAndCompositionMap",
	"OutputUpscaledColor",
};
*/

/*
#define ToStrAnd( s, S ) if (s & S) out += "|" #S

std::string ToStr(D3D12_RESOURCE_STATES s) {
	std::string out = "";
	ToStrAnd(s, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	ToStrAnd(s, D3D12_RESOURCE_STATE_INDEX_BUFFER);
	ToStrAnd(s, D3D12_RESOURCE_STATE_RENDER_TARGET);
	ToStrAnd(s, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	ToStrAnd(s, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_DEPTH_READ);
	ToStrAnd(s, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_STREAM_OUT);
	ToStrAnd(s, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
	ToStrAnd(s, D3D12_RESOURCE_STATE_COPY_DEST);
	ToStrAnd(s, D3D12_RESOURCE_STATE_COPY_SOURCE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_RESOLVE_DEST);
	ToStrAnd(s, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
	ToStrAnd(s, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE);

	return out;
}
*/
/*
void* ResourceBarrier_o = nullptr;
void STDMETHODCALLTYPE ResourceBarrier_hk(
	ID3D12CommandList* This,
	_In_  UINT NumBarriers,
	_In_reads_(NumBarriers)  const D3D12_RESOURCE_BARRIER* pBarriers) {
	//spdlog::trace(__FUNCTION__);
	if (in_fsr2) {
		//
		//spdlog::trace(__FUNCTION__ " in fsr2");
		for (size_t j = 0; j < NumBarriers; j++) {

			//if (pBarriers[j].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
			//	continue;
			for (size_t i = 0; i < (sizeof(objs_i_care) / sizeof(objs_i_care[0])); i++) {
				if (objs_i_care[i] == pBarriers[j].Transition.pResource) {
					spdlog::debug("I care {} | {} -> {}", std::string_view(strs[i]), ToStr(pBarriers[j].Transition.StateBefore), ToStr(pBarriers[j].Transition.StateAfter));
				}
			}
			spdlog::trace("{} | {} -> {}", PVOID(pBarriers[j].Transition.pResource), ToStr(pBarriers[j].Transition.StateBefore), ToStr(pBarriers[j].Transition.StateAfter));
		}
	}
	reinterpret_cast<decltype(&ResourceBarrier_hk)>(ResourceBarrier_o)(This, NumBarriers, pBarriers);
}
*/

void* g_UpscaledColor = nullptr;
void* g_MVecs = nullptr;
void* g_Depth = nullptr;
float g_MVecsScale[2] = {
	0, 0
};

void* g_ColorDuplicate = nullptr;

void Execute_hk(uintptr_t thiz, uintptr_t a2, uintptr_t a3) {
	auto retaddr = PVOID(uintptr_t(_ReturnAddress()) - g_Starfield);
#ifndef NDEBUG
	spdlog::trace("FSR2UpscaleRenderPass::Execute({}, {}, {}) from {}", PVOID(thiz), PVOID(a2), PVOID(a3), retaddr);
#endif

	//in_fsr2 = true;

	extern ID3D12Device* g_pdevice;

	// E8 ?? ?? ?? ?? BA 02 9E 65 07
	using RenderPass_a3_shader_getSlot_fn = uintptr_t(__fastcall*)(uintptr_t a3, uint32_t slot);
	auto RenderPass_a3_shader_getSlot = RenderPass_a3_shader_getSlot_fn(g_Starfield + 0x337C608);

	auto InputColor = RenderPass_a3_shader_getSlot(a3, 0x5C95100);
	auto InputDepth = RenderPass_a3_shader_getSlot(a3, 0x5C82201);
	auto InputMotionVectors = RenderPass_a3_shader_getSlot(a3, 0x5C81B02);
	auto InputReactiveMap = RenderPass_a3_shader_getSlot(a3, 0x5C94F03);
	auto TransparencyAndCompositionMap = RenderPass_a3_shader_getSlot(a3, 0x5C95004);

	auto OutputUpscaledColor = RenderPass_a3_shader_getSlot(a3, 0x5C94B0E);

	if (!InputColor || !InputDepth || !InputMotionVectors || !InputReactiveMap || !TransparencyAndCompositionMap || !OutputUpscaledColor)
		return;

	// Start recording idk
	using sub_14337C318_fn = uintptr_t(__fastcall*)(uintptr_t a1);
	auto sub_14337C318 = sub_14337C318_fn(g_Starfield + 0x337C318);
	auto v11 = sub_14337C318(a2);
	auto commandlist_wrapper = v11;
	auto commandlist = *(ID3D12GraphicsCommandList**)(commandlist_wrapper + 16);

	// No debug names sadly
	// spdlog::debug("`{}` `{}` `{}` `{}` `{}` `{}`", our_SlotGetName(InputColor), our_SlotGetName(InputDepth), our_SlotGetName(InputMotionVectors), our_SlotGetName(InputReactiveMap), our_SlotGetName(TransparencyAndCompositionMap), our_SlotGetName(OutputUpscaledColor));

#ifndef NDEBUG
	spdlog::debug("InputColor:{} InputDepth:{} InputMotionVectors:{} InputReactiveMap:{} TransparencyAndCompositionMap:{} | OutputUpscaledColor:{}", (PVOID)SlotGetCResource(InputColor), (PVOID)SlotGetCResource(InputDepth), (PVOID)SlotGetCResource(InputMotionVectors), (PVOID)SlotGetCResource(InputReactiveMap), (PVOID)SlotGetCResource(TransparencyAndCompositionMap), (PVOID)SlotGetCResource(OutputUpscaledColor));
#endif

	//HookFSR2UpscaleRenderPass_vtbl.Execute(thiz, a2, a3);

	// _RDX = *(_QWORD *)(*(_QWORD *)(a2 + 8i64 * *(unsigned int *)(a2 + 456) + 408) + 48i64);
	auto _RDX = *(float**)(*(uintptr_t*)(a2 + 8i64 * *(unsigned int*)(a2 + 456) + 408) + 48i64);
	float jitter[] = {
		_RDX[0x0E8 / 4], _RDX[0x0EC / 4]
	};
	// spdlog::trace("rdx+0E8/0EC {}x{}", _RDX[0x0E8/4], _RDX[0x0E8/4]);

	// E8 ?? ?? ?? ?? 49 8B 48 38 BA 16 00 00 00
	using GetViewportInfoFromIdPtr_fn = void* (__fastcall*)(uint32_t* viewport);
	auto GetViewportInfoFromIdPtr = GetViewportInfoFromIdPtr_fn(g_Starfield + 0x33E4CA0);

	// those names I pulled from my ass idfk
	auto a2_current_viewport_id = *(unsigned int*)(a2 + 0x1C8);
	auto a2_viewport_array = (uintptr_t*)(a2 + 0x198);
	auto a2_current_viewport = a2_viewport_array[a2_current_viewport_id];
	uint32_t a2_current_viewport_id2 = *(uint32_t*)(a2_current_viewport + 32);

	auto qword_145049EB8 = *(uintptr_t*)(g_Starfield + 0x5049EB8);
	auto qword_145049F90 = *(uintptr_t*)(g_Starfield + 0x5049F90);
	auto v15 = *(uintptr_t*)(qword_145049EB8 + 256);
	auto _RAX = *(uintptr_t*)(v15 + 968);
	auto _RDX_ = 3i64
		* *(unsigned int*)(*(uintptr_t*)(v15 + 64)
			+ 4
			* (*(uint32_t*)(*(uintptr_t*)(*((uintptr_t*)qword_145049F90 + 37) + 968i64)
				+ 24i64
				* *(unsigned int*)(*(uintptr_t*)(*((uintptr_t*)qword_145049F90 + 37) + 64i64)
					+ 4 * (*(uint32_t*)(a2_current_viewport + 36) & 0xFFFFFFi64))
				+ 4) & 0xFFFFFFi64));
	auto rdf = (float*)(_RAX + (_RDX_ * 8));
#ifndef NDEBUG
	spdlog::debug("_RDX_: {} {} {} {} | {} {}", rdf[0], rdf[1], rdf[2], rdf[3], rdf[4], rdf[5]);
#endif

	// Now we evaluate the DLSS...
	// I hate my life...
	auto opaque = g_Opaque;
	auto ovtbl = *(uintptr_t**)opaque;
	auto NVSDK_NGX_Parameter_SetD3d12Resource = NVSDK_NGX_Parameter_SetD3d12Resource_fn(ovtbl[1]);
	auto NVSDK_NGX_Parameter_SetF = NVSDK_NGX_Parameter_SetF_fn(ovtbl[6]);
	auto NVSDK_NGX_Parameter_SetI = NVSDK_NGX_Parameter_SetI_fn(ovtbl[3]);
	auto NVSDK_NGX_Parameter_SetUI = NVSDK_NGX_Parameter_SetUI_fn(ovtbl[4]);
	
	// First let's set in and out...
#ifndef NDEBUG
	auto ic = SlotGetCResource(InputColor);
	auto ouc = SlotGetCResource(OutputUpscaledColor);
	auto icd = ic->GetDesc();
	auto oucd = ouc->GetDesc();
	spdlog::trace("Formats: {}x{} {} | {}x{} {}", icd.Width, icd.Height, +icd.Format, oucd.Width, oucd.Height, +oucd.Format);
#endif

	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Color", SlotGetCResource(InputColor));
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Output", SlotGetCResource(OutputUpscaledColor));
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Color", ic_wrap);
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Output", ouc_wrap);

#ifndef NDEBUG
	// Now Depth and MotionVectors
	auto id = SlotGetCResource(InputDepth);
	auto imv = SlotGetCResource(InputMotionVectors);
	auto idd = id->GetDesc();
	auto imvd = imv->GetDesc();
	spdlog::trace("Formats: {}x{} {} | {}x{}[{}] {} {}", idd.Width, idd.Height, +idd.Format, imvd.Width, imvd.Height, +imvd.Dimension, +imvd.Format, imvd.Alignment);
#endif

	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Depth", SlotGetCResource(InputDepth));
	//spdlog::trace("id_wrap = {} | imv_wrap = {}", PVOID(id_wrap), PVOID(imv_wrap));
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Depth", depth_wrap_non_typeless);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "MotionVectors", SlotGetCResource(InputMotionVectors));
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "MotionVectors", imv_wrap);
	//NVSDK_NGX_Parameter_SetF(opaque, "MV.Scale.X", 1);
	//NVSDK_NGX_Parameter_SetF(opaque, "MV.Scale.Y", 1);
	g_MVecsScale[0] = *(uint32_t*)(InputColor + 4) * -0.5f;
	g_MVecsScale[1] = *(uint32_t*)(InputColor + 8) * 0.5f;
	NVSDK_NGX_Parameter_SetF(opaque, "MV.Scale.X", *(uint32_t*)(InputColor + 4) * -0.5f);
	NVSDK_NGX_Parameter_SetF(opaque, "MV.Scale.Y", *(uint32_t*)(InputColor + 8) * 0.5f);

	g_MVecs = SlotGetCResource(InputMotionVectors);
	g_Depth = SlotGetCResource(InputDepth);

	// TODO: Jitters
	NVSDK_NGX_Parameter_SetF(opaque, "Jitter.Offset.X", jitter[0]);
	NVSDK_NGX_Parameter_SetF(opaque, "Jitter.Offset.Y", jitter[1]);
	//NVSDK_NGX_Parameter_SetF(opaque, "Jitter.Offset.X", 0);
	//NVSDK_NGX_Parameter_SetF(opaque, "Jitter.Offset.Y", 0);

	// Optional and deprecated?...
	NVSDK_NGX_Parameter_SetF(opaque, "Sharpness", 0.05f);
	//NVSDK_NGX_Parameter_SetF(opaque, "Sharpness", 0.01f);
	//NVSDK_NGX_Parameter_SetF(opaque, "Sharpness", 0.7f);

	using _DWORD = uint32_t;
	using _QWORD = uintptr_t;
	//auto qword_145049F90 = *(uintptr_t*)(g_Starfield + 0x5049F90);
	auto v16 = (*(_DWORD*)(*(_QWORD*)(*((_QWORD*)qword_145049F90 + 37) + 968i64)
		+ 24i64
		* *(unsigned int*)(*(_QWORD*)(*((_QWORD*)qword_145049F90 + 37) + 64i64)
			+ 4 * (*(_DWORD*)(a2_current_viewport + 36) & 0xFFFFFFi64))
		+ 4) & 0xFFFFFFi64) != 0;

	auto qword_145049FB0 = *(uintptr_t*)(g_Starfield + 0x5049FB0);
	auto v21 = *((_QWORD*)qword_145049FB0 + 39);
	auto v22 = *(_QWORD*)(*(_QWORD*)(*(_QWORD*)(v21 + 16) + 8i64) + 8 * (a2_current_viewport_id2 & 0xFFFFFF));

	auto v23 = *(_QWORD*)(*(_QWORD*)(v21 + 8) + 8i64);
	//auto bunk = *(char*)((a2_current_viewport_id2 & 0xFFFFFF) + v23);
	//spdlog::debug("Reset idk maybe: {} {}", +v16, +bunk);

	// TODO: reset
	//static int reset_stuff = 0;
	//NVSDK_NGX_Parameter_SetI(opaque, "Reset", (reset_stuff++ % 500) == 0);
	NVSDK_NGX_Parameter_SetI(opaque, "Reset", *(char*)((a2_current_viewport_id2 & 0xFFFFFF) + v23));
	*(char*)((a2_current_viewport_id2 & 0xFFFFFF) + v23) = 0;

	// Meme
	//auto tm = SlotGetCResource(TransparencyAndCompositionMap);
	//auto irm = SlotGetCResource(InputReactiveMap);
	//auto tmd = tm->GetDesc();
	//auto irmd = irm->GetDesc();
	//spdlog::trace("Formats: {}x{} {} | {}x{} {}", tmd.Width, tmd.Height, +tmd.Format, irmd.Width, irmd.Height, +irmd.Format);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "TransparencyMask", SlotGetCResource(TransparencyAndCompositionMap));
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "TransparencyMask", nullptr);
	// NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "ExposureTexture", SlotGetCResource(InputReactiveMap));
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "ExposureTexture", nullptr);


	// Not required at all...
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "DLSS.Input.Bias.Current.Color.Mask", SlotGetCResource(TransparencyAndCompositionMap));
	//NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "DLSS.Input.Bias.Current.Color.Mask", SlotGetCResource(InputReactiveMap));
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "DLSS.Input.Bias.Current.Color.Mask", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Albedo", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Roughness", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Metallic", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Specular", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Subsurface", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Normals", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.ShadingModelId", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.MaterialId", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.8", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.9", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.10", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.11", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.12", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.13", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.14", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "GBuffer.Attrib.15", nullptr);
	NVSDK_NGX_Parameter_SetUI(opaque, "TonemapperType", 0);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "MotionVectors3D", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "IsParticleMask", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "AnimatedTextureMask", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "DepthHighRes", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "Position.ViewSpace", nullptr);
	NVSDK_NGX_Parameter_SetF(opaque, "FrameTimeDeltaInMsec", 0);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "RayTracingHitDistance", nullptr);
	NVSDK_NGX_Parameter_SetD3d12Resource(opaque, "MotionVectorsReflection", nullptr);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Color.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Color.Subrect.Base.Y", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Depth.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Depth.Subrect.Base.Y", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.MV.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.MV.Subrect.Base.Y", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Translucency.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Translucency.Subrect.Base.Y", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Bias.Current.Color.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Input.Bias.Current.Color.Subrect.Base.Y", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Output.Subrect.Base.X", 0);
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Output.Subrect.Base.Y", 0);

	NVSDK_NGX_Parameter_SetF(opaque, "DLSS.Pre.Exposure", 1);
	NVSDK_NGX_Parameter_SetF(opaque, "DLSS.Exposure.Scale", 1);

	NVSDK_NGX_Parameter_SetI(opaque, "DLSS.Indicator.Invert.X.Axis", 0);
	NVSDK_NGX_Parameter_SetI(opaque, "DLSS.Indicator.Invert.Y.Axis", 0);

	// viewport_info gets changed in orig if needed
	// allocated 0x20
	// first 4 are sizes
	// @8 - flags
	// @16 - fsr context (0x10260)
	auto viewport_info = GetViewportInfoFromIdPtr(&a2_current_viewport_id2);
	auto viewport_info_ctx_ = *(uintptr_t*)viewport_info;
	auto ctx = *(STKDLSSCtx**)(viewport_info_ctx_ + 16);

	auto viewport_info_sizes = *(uint16_t**)viewport_info;
	// Required
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Render.Subrect.Dimensions.Width", *(uint32_t*)(InputColor + 4));
	NVSDK_NGX_Parameter_SetUI(opaque, "DLSS.Render.Subrect.Dimensions.Height", *(uint32_t*)(InputColor + 8));

	// 48 83 EC 28 48 8B 89 C0 01 00 00 33
	/*
	using GetCommandList_wrapper_fn = uintptr_t(__fastcall*)(uintptr_t);
	auto GetCommandList_wrapper = GetCommandList_wrapper_fn(g_Starfield + 0x337C318);
	auto commandlist_wrapper = GetCommandList_wrapper(a2);
	auto commandlist = *(ID3D12GraphicsCommandList**)(commandlist_wrapper + 16);
	*/

	static auto NVSDK_NGX_D3D12_EvaluateFeature = NVSDK_NGX_D3D12_EvaluateFeature_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_EvaluateFeature"));
	auto ret = NVSDK_NGX_D3D12_EvaluateFeature(commandlist, ctx->dlss_handle, g_Opaque, nullptr);
#ifndef NDEBUG
	spdlog::trace("NVSDK_NGX_D3D12_EvaluateFeature(DLSS)({}, {}, {}) = {} | {}x{} {}x{} | {}x{}", PVOID(commandlist), PVOID(ctx->dlss_handle), PVOID(g_Opaque), PVOID(ret), viewport_info_sizes[0], viewport_info_sizes[1], viewport_info_sizes[2], viewport_info_sizes[3], *(uint32_t*)(InputColor + 4), *(uint32_t*)(InputColor + 8));
#endif
	// debug
	//commandlist->CopyResource(SlotGetCResource(OutputUpscaledColor), SlotGetCResource(InputColor));

	/*
	if (!g_ColorDuplicate) {
		ID3D12Resource* tmp;
		auto desc = SlotGetCResource(OutputUpscaledColor)->GetDesc();
		D3D12_HEAP_PROPERTIES prop;
		ZeroMemory(&prop, sizeof(prop));
		prop.Type = D3D12_HEAP_TYPE_DEFAULT;
		g_pdevice->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&tmp)
		);
		g_ColorDuplicate = tmp;
	}
	commandlist->CopyResource((ID3D12Resource*)g_ColorDuplicate, SlotGetCResource(OutputUpscaledColor));
	*/

	// 44 88 4C 24 20 4C 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E8
	using barrier_shit_fn = void(__fastcall*)(void*, uintptr_t commandlist_wrap, uintptr_t caller_a3, char reset_qm);
	auto barrier_shit = barrier_shit_fn(g_Starfield + 0x3330F70);

	barrier_shit(PVOID(v22), commandlist_wrapper, a3, v16);
	
	//in_fsr2 = false;
}

// This was produce inputs...
// I spent like 3 days on this ffs
//constexpr uintptr_t OFFSET = 0x47682B8;
constexpr uintptr_t OFFSET = 0x4768300;

void HookFSR2UpscaleRenderPass() {
	// Change VTable

	auto ptr = (void**)(g_Starfield + OFFSET);
	memcpy(&HookFSR2UpscaleRenderPass_vtbl, ptr, sizeof(HookFSR2UpscaleRenderPass_vtbl));
	// Now we virtual protect up in this bitch
	DWORD oldProt = PAGE_EXECUTE_READ;
	VirtualProtect(ptr, sizeof(HookFSR2UpscaleRenderPass_vtbl), PAGE_EXECUTE_READWRITE, &oldProt);
#ifndef NDEBUG
	ptr[0] = dtor_hk;
	ptr[1] = vfunc1_hk;
	ptr[2] = vfunc2_hk;
#endif
	ptr[3] = vfunc3_hk;
#ifndef NDEBUG
	ptr[4] = vfunc4_hk;
#endif

#ifndef NDEBUG
	// noops
	ptr[5] = vfunc5_hk;
	ptr[6] = vfunc6_hk;
#endif
	
	ptr[7] = Execute_hk;
	VirtualProtect(ptr, sizeof(HookFSR2UpscaleRenderPass_vtbl), oldProt, nullptr);

	// Now we hook FSR2 related functions
	// Jitter and Phase are in fucking SetupRenderScene or something like that
	// get_jitter_phase_stuff - 0x3330608
	// 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B C2 4C

#ifndef NDEBUG
	spdlog::info(__FUNCTION__ " success!");
#endif
}

void UnHookFSR2UpscaleRenderPass() {
	auto ptr = (void**)(g_Starfield + OFFSET);
	DWORD oldProt = PAGE_EXECUTE_READ;
	VirtualProtect(ptr, sizeof(HookFSR2UpscaleRenderPass_vtbl), PAGE_EXECUTE_READWRITE, &oldProt);
	memcpy(ptr, &HookFSR2UpscaleRenderPass_vtbl, sizeof(HookFSR2UpscaleRenderPass_vtbl));
	VirtualProtect(ptr, sizeof(HookFSR2UpscaleRenderPass_vtbl), oldProt, nullptr);

#ifndef NDEBUG
	spdlog::info(__FUNCTION__ " success!");
#endif
}