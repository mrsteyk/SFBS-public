// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#include <cstdint>
#include <string>

#include <MinHook.h>
#include <spdlog/spdlog.h>

#include <d3d12.h>

uintptr_t g_Starfield = 0;

bool in_fsr2 = false;

void* objs_i_care[15];

const char* strs[15] = {
    "InputColor",
    "InputDepth",
    "InputMotionVectors",
    "InputReactiveMap",
    "TransparencyAndCompositionMap",
    "OutputUpscaledColor",

    "Slot",
    "v5",
    "v6",
    "v7",
    "v148",
    "v149",
    "v150",
    "v145",
    "v130"
};

inline ID3D12Resource2* SlotGetCResource(uintptr_t slot) {
    if (!slot)
        return nullptr;

    return *(ID3D12Resource2**)(*(uintptr_t*)(slot + 0x48) + 0xA8);
}

#define ToStrAnd( s, S ) if (s & S) out += "|" #S

std::string ToStr(D3D12_RESOURCE_STATES s) {
    std::string out = "";
    /*
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER	= 0x1,
        D3D12_RESOURCE_STATE_INDEX_BUFFER	= 0x2,
        D3D12_RESOURCE_STATE_RENDER_TARGET	= 0x4,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS	= 0x8,
        D3D12_RESOURCE_STATE_DEPTH_WRITE	= 0x10,
        D3D12_RESOURCE_STATE_DEPTH_READ	= 0x20,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE	= 0x40,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE	= 0x80,
        D3D12_RESOURCE_STATE_STREAM_OUT	= 0x100,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT	= 0x200,
        D3D12_RESOURCE_STATE_COPY_DEST	= 0x400,
        D3D12_RESOURCE_STATE_COPY_SOURCE	= 0x800,
        D3D12_RESOURCE_STATE_RESOLVE_DEST	= 0x1000,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE	= 0x2000,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE	= 0x400000,
        D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE	= 0x1000000,
    */
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

void* ResourceBarrier_o = nullptr;
void STDMETHODCALLTYPE ResourceBarrier_hk(
    ID3D12CommandList* This,
    _In_  UINT NumBarriers,
    _In_reads_(NumBarriers)  const D3D12_RESOURCE_BARRIER* pBarriers) {
    //spdlog::trace(__FUNCTION__);
    if (in_fsr2) {
        //
        spdlog::trace(__FUNCTION__ " in fsr2");
        for (size_t j = 0; j < NumBarriers; j++) {
            if (pBarriers[j].Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
                continue;
            bool found = false;
            for (size_t i = 0; i < (sizeof(objs_i_care) / sizeof(objs_i_care[0])); i++) {
                if (objs_i_care[i] == pBarriers[j].Transition.pResource) {
                    spdlog::debug("I care {} | {} -> {}", std::string_view(strs[i]), ToStr(pBarriers[j].Transition.StateBefore), ToStr(pBarriers[j].Transition.StateAfter));
                    found = true;
                }
            }
            if(!found)
                spdlog::trace("{} | {} -> {}", PVOID(pBarriers[j].Transition.pResource), ToStr(pBarriers[j].Transition.StateBefore), ToStr(pBarriers[j].Transition.StateAfter));
        }
    }
    reinterpret_cast<decltype(&ResourceBarrier_hk)>(ResourceBarrier_o)(This, NumBarriers, pBarriers);
}

void* callfsr2_o = nullptr;
void callfsr2(uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    using RenderPass_a3_shader_getSlot_fn = uintptr_t(__fastcall*)(uintptr_t a3, uint32_t slot);
    auto RenderPass_a3_shader_getSlot = RenderPass_a3_shader_getSlot_fn(g_Starfield + 0x337C608);

    auto InputColor = RenderPass_a3_shader_getSlot(a3, 0x5C95100);
    auto InputDepth = RenderPass_a3_shader_getSlot(a3, 0x5C82201);
    auto InputMotionVectors = RenderPass_a3_shader_getSlot(a3, 0x5C81B02);
    auto InputReactiveMap = RenderPass_a3_shader_getSlot(a3, 0x5C94F03);
    auto TransparencyAndCompositionMap = RenderPass_a3_shader_getSlot(a3, 0x5C95004);

    auto OutputUpscaledColor = RenderPass_a3_shader_getSlot(a3, 0x5C94B0E);

    objs_i_care[0] = SlotGetCResource(InputColor);
    objs_i_care[1] = SlotGetCResource(InputDepth);
    objs_i_care[2] = SlotGetCResource(InputMotionVectors);
    objs_i_care[3] = SlotGetCResource(InputReactiveMap);
    objs_i_care[4] = SlotGetCResource(TransparencyAndCompositionMap);
    objs_i_care[5] = SlotGetCResource(OutputUpscaledColor);

    auto Slot = RenderPass_a3_shader_getSlot(a3, 0x5C95205);
    auto v5 = RenderPass_a3_shader_getSlot(a3, 0x5C95306);
    auto v6 = RenderPass_a3_shader_getSlot(a3, 0x5C95407);
    auto v7 = RenderPass_a3_shader_getSlot(a3, 0x5C95508);
    auto v148 = RenderPass_a3_shader_getSlot(a3, 0x5C95609);
    auto v149 = RenderPass_a3_shader_getSlot(a3, 0x5C9570A);
    auto v150 = RenderPass_a3_shader_getSlot(a3, 0x5C9580B);
    auto v145 = RenderPass_a3_shader_getSlot(a3, 0x5C9590C);
    auto v130 = RenderPass_a3_shader_getSlot(a3, 0x5C95A0D);

    objs_i_care[6] = SlotGetCResource(Slot);
    objs_i_care[7] = SlotGetCResource(v5);
    objs_i_care[8] = SlotGetCResource(v7);
    objs_i_care[9] = SlotGetCResource(v148);
    objs_i_care[10] = SlotGetCResource(v149);
    objs_i_care[11] = SlotGetCResource(v150);
    objs_i_care[12] = SlotGetCResource(Slot);
    objs_i_care[13] = SlotGetCResource(v145);
    objs_i_care[14] = SlotGetCResource(v130);

    for (size_t i = 0; i < (sizeof(objs_i_care) / sizeof(objs_i_care[0])); i++) {
        spdlog::debug("{} | {}", std::string_view(strs[i]), PVOID(objs_i_care[i]));
    }

    static bool once = false;
    if (!once) {
        once = true;

        using GetCommandList_wrapper_fn = uintptr_t(__fastcall*)(uintptr_t);
        auto GetCommandList_wrapper = GetCommandList_wrapper_fn(g_Starfield + 0x337C318);
        auto commandlist_wrapper = GetCommandList_wrapper(a2);
        auto commandlist = *(void**)(commandlist_wrapper + 16);

        auto vtbl = *(void***)commandlist;
        MH_CreateHook(vtbl[26], ResourceBarrier_hk, &ResourceBarrier_o);
        MH_EnableHook(vtbl[26]);
    }

    in_fsr2 = true;
    spdlog::debug("Before FSR2");
    reinterpret_cast<decltype(&callfsr2)>(callfsr2_o)(a1, a2, a3);
    spdlog::debug("After FSR2");
    in_fsr2 = false;
}

void* fpRegisterResource_o = nullptr;
uintptr_t fpRegisterResource(uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    auto ret = reinterpret_cast<decltype(&fpRegisterResource)>(fpRegisterResource_o)(a1, a2, a3);
    
    //spdlog::debug("Register: {} -> {}", *(PVOID*)a2, *(PVOID*)a3);

    for (size_t i = 0; i < (sizeof(objs_i_care) / sizeof(objs_i_care[0])); i++) {
        if ((*(PVOID*)a2) == objs_i_care[i]) {
            auto v3 = *(__int64***)(a1 + 96);
            auto v9 = 7i64 * *(int*)a3;
            auto v10 = (uintptr_t)&v3[v9 + 60547];

            //auto res = ;

            spdlog::debug("Register care: {} {} -> {}", strs[i], *(PVOID*)a2, *(int*)a3);
        }
    }

    return ret;
}

void* ffxFsr2ContextDispatch_o = nullptr;
uintptr_t ffxFsr2ContextDispatch(uintptr_t a1, uintptr_t a2) {

    static bool once = false;
    if (!once) {
        once = true;
        auto p = *(PVOID*)(a1 + 0x18 + 0x20);
        spdlog::debug("RegisterResource: {}", p);
        MH_CreateHook(p, &fpRegisterResource, &fpRegisterResource_o);
        MH_EnableHook(p);
    }

    return reinterpret_cast<decltype(&ffxFsr2ContextDispatch)>(ffxFsr2ContextDispatch_o)(a1, a2);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        AllocConsole();
        FILE* file = nullptr;
        freopen_s(&file, "CONIN$", "r", stdin);
        freopen_s(&file, "CONOUT$", "w", stdout);
        freopen_s(&file, "CONOUT$", "w", stderr);

        // Set logger level to trace
        spdlog::set_level(spdlog::level::trace);

        MH_Initialize();

        g_Starfield = (uintptr_t)GetModuleHandleA(nullptr);
        auto callfsr2_ = PVOID(g_Starfield + 0x33E4D68);
        MH_CreateHook(callfsr2_, &callfsr2, &callfsr2_o);
        MH_EnableHook(callfsr2_);

        auto ffxFsr2ContextDispatch_ = PVOID(GetProcAddress((HMODULE)g_Starfield, "ffxFsr2ContextDispatch"));
        MH_CreateHook(ffxFsr2ContextDispatch_, &ffxFsr2ContextDispatch, &ffxFsr2ContextDispatch_o);
        MH_EnableHook(ffxFsr2ContextDispatch_);

        spdlog::info("ffxFsr2ContextDispatch - {}", ffxFsr2ContextDispatch_);
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

