#include "core.hh"

#include <MinHook.h>

#define INITGUID
//#define CINTERFACE
#include <dxgi1_4.h>
#include <d3d12.h>

#include "dlss.hh"

#include "renderpass_hook.hh"

void* CreateSwapChainForHwnd_orig = nullptr;
void* Present_orig = nullptr;

IDXGISwapChain3* g_pSwapChain = nullptr;
ID3D12Device* g_pdevice = nullptr;

HWND g_window = nullptr;
WNDPROC o_wndproc = nullptr;

#include "./vendor/ImGui/imgui.h"
#include "./vendor/ImGui/backends/imgui_impl_dx12.h"
#include "./vendor/ImGui/backends/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static bool b_showmenu = false;

__int64 __stdcall hk_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == WM_KEYDOWN) {
		if (wParam == VK_INSERT)
			b_showmenu = !b_showmenu;
	}

	if (b_showmenu) {
		ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam);
	}

	return CallWindowProc(o_wndproc, hwnd, uMsg, wParam, lParam);
}

std::once_flag present;
struct {
    // swapchain related stuff
    struct _FrameContext {
        ID3D12CommandAllocator* CommandAllocator;
        ID3D12CommandAllocator* CommandAllocator_g;
        ID3D12Resource* Resource;
        D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
    };
    size_t buffer_count;
    _FrameContext* buffers_ptr = nullptr;

    ID3D12DescriptorHeap* DescriptorHeapBackBuffers;
    ID3D12DescriptorHeap* DescriptorHeapImGuiRender;
    ID3D12GraphicsCommandList* CommandList;
    ID3D12CommandQueue* CommandQueue = nullptr;
} presents_stuff;
// TODO: intercept any queue...
void* oExecuteCommandLists = nullptr;
void hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists) {
    if (!presents_stuff.CommandQueue)
        if (*(uintptr_t*)queue != 0xFEEEFEEEFEEEFEEE)
            presents_stuff.CommandQueue = queue;

    reinterpret_cast<decltype(&hkExecuteCommandLists)>(oExecuteCommandLists)(queue, NumCommandLists, ppCommandLists);
}
// NvAPI fun...
static auto nvapi_mod = (HMODULE)nullptr;
using QueryIface_fn = void* (__fastcall*)(unsigned int);
static auto nv_query_iface = QueryIface_fn(nullptr);
using CallStart_fn = void(__fastcall*)(unsigned int, void*);
using CallEnd_fn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, uintptr_t);
static auto nv_call_start = CallStart_fn(nullptr);
static auto nv_call_end = CallEnd_fn(nullptr);
struct _NV_SET_SLEEP_MODE_PARAMS
{
    uint32_t version;                                       //!< (IN) Structure version
    uint8_t bLowLatencyMode;                               //!< (IN) Low latency mode enable/disable.
    uint8_t bLowLatencyBoost;                              //!< (IN) Request maximum GPU clock frequency regardless of workload.
    uint32_t minimumIntervalUs;                             //!< (IN) Minimum frame interval in microseconds. 0 = no frame rate limit.
    uint8_t bUseMarkersToOptimize;                         //!< (IN) Allow latency markers to be used for runtime optimizations.
    uint8_t rsvd[31];                                      //!< (IN) Reserved. Must be set to 0s.
};
inline uintptr_t NvAPI_D3D_SetSleepMode(void* device, _NV_SET_SLEEP_MODE_PARAMS* params) {
    const auto iface_id = 0xAC1CA9E0ull;
    static auto iface = decltype(&NvAPI_D3D_SetSleepMode)(nullptr);
    if (!iface) {
        iface = decltype(iface)(nv_query_iface(iface_id));
    }

    if (iface) {
        uintptr_t v7 = 0;
        if (nv_call_start)
            nv_call_start(iface_id, &v7);
        auto v6 = iface(device, params);
        if (nv_call_end)
            nv_call_end(iface_id, v7, v6);

        return v6;
    }

    return 0xFFFFFFFD;
}
inline uintptr_t NvAPI_D3D_Sleep(void* a1) {
    const auto iface_id = 0x852CD1D2ull;
    static auto iface = decltype(&NvAPI_D3D_Sleep)(nullptr);
    if (!iface) {
        iface = decltype(iface)(nv_query_iface(iface_id));
    }

    if (iface) {
        uintptr_t v7 = 0;
        if (nv_call_start)
            nv_call_start(iface_id, &v7);
        auto v6 = iface(a1);
        if (nv_call_end)
            nv_call_end(iface_id, v7, v6);

        return v6;
    }

    return 0xFFFFFFFD;
}
static struct {
    bool lowlat = true;
    bool boost = true;
} reflex_params;
int g_DLSS_preset = 0;
bool g_DLSSG_enabled = true;
ID3D12Resource* outputinterp = nullptr;
D3D12_CPU_DESCRIPTOR_HANDLE texCPUHandle;
D3D12_GPU_DESCRIPTOR_HANDLE texGPUHandle;
HRESULT STDMETHODCALLTYPE Present_hk(
    IDXGISwapChain3* This,
    /* [in] */ UINT SyncInterval,
    /* [in] */ UINT Flags) {
    // I would like to tell you my vision:
    // This hook is required because we need to get FULL framebuffer image for DLSSG
    // Streamline should do this automagically, but I will use raw nvngx_dlssg, so manual hook ftw
    // The problem arises when we want to get a HUD image
    // I can write a CS for that (I have upscaled image from my FSR2 pass hooks and I have full image here, but it might be a problem because HDR?) but is there a better way?...
    // Btw this is like a paste of DrNSeven's stuff (which in turn was pasted from someone on UC?)
    // But with pasted once_flag stuff from gaypig (I don't remember to which name did he change, so I list the original)

    //spdlog::debug("Present from {}", PVOID(uintptr_t(_ReturnAddress()) - g_Starfield));

    // nvapi for reflex...
    if (!nvapi_mod) {
        nvapi_mod = GetModuleHandleA("nvapi64.dll");
#ifndef NDEBUG
        spdlog::warn("nvapi_mod was 0! Now: {}", PVOID(nvapi_mod));
#endif

        nv_query_iface = QueryIface_fn(GetProcAddress(nvapi_mod, "nvapi_QueryInterface"));
        nv_call_start = decltype(nv_call_start)(nv_query_iface(0x33C7358Cu));
        nv_call_end = decltype(nv_call_end)(nv_query_iface(0x593E8644u));
        if (!nv_call_start || !nv_call_end) {
            nv_call_start = decltype(nv_call_start)(nullptr);
            nv_call_end = decltype(nv_call_end)(nullptr);
        }
#ifndef NDEBUG
        spdlog::warn("nvapi: {} {} {}", PVOID(nv_query_iface), PVOID(nv_call_start), PVOID(nv_call_end));
#endif
    }

    // Now I will need to get the backbuffer and make IMGUI objects...
	std::call_once(present, [&] {
#ifndef NDEBUG
        spdlog::debug("Present_hk::once called!");
#endif
        g_pSwapChain->GetDevice(__uuidof(g_pdevice), (void**)(&g_pdevice));
        //InitDLSS(g_pdevice);

		//shit way of doing it
		o_wndproc = (WNDPROC)(SetWindowLongPtrA(g_window, GWLP_WNDPROC, uintptr_t(hk_wndproc)));

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        // HELL, ...
        // Needless to say you might experience a memory leak of DX12 objects!
        DXGI_SWAP_CHAIN_DESC Desc;
        g_pSwapChain->GetDesc(&Desc);

        presents_stuff.buffer_count = Desc.BufferCount;
        presents_stuff.buffers_ptr = (decltype(presents_stuff.buffers_ptr))malloc(presents_stuff.buffer_count * sizeof(presents_stuff.buffers_ptr[0]));

        D3D12_DESCRIPTOR_HEAP_DESC DescriptorImGuiRender = {};
        DescriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        DescriptorImGuiRender.NumDescriptors = presents_stuff.buffer_count + 1;
        DescriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        g_pdevice->CreateDescriptorHeap(&DescriptorImGuiRender, IID_PPV_ARGS(&presents_stuff.DescriptorHeapImGuiRender));

        ID3D12CommandAllocator* allocator;
        //ID3D12CommandAllocator* allocator_g;
        g_pdevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));

        g_pdevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&presents_stuff.CommandList));

        D3D12_DESCRIPTOR_HEAP_DESC DescriptorBackBuffers;
        DescriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        DescriptorBackBuffers.NumDescriptors = presents_stuff.buffer_count;
        DescriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        DescriptorBackBuffers.NodeMask = 1;

        g_pdevice->CreateDescriptorHeap(&DescriptorBackBuffers, IID_PPV_ARGS(&presents_stuff.DescriptorHeapBackBuffers));

        const auto RTVDescriptorSize = g_pdevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = presents_stuff.DescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

        // We create everything for every single
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        for (size_t i = 0; i < presents_stuff.buffer_count; i++) {
            presents_stuff.buffers_ptr[i].CommandAllocator = allocator;
            //presents_stuff.buffers_ptr[i].CommandAllocator_g = allocator_g;
            g_pdevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&presents_stuff.buffers_ptr[i].CommandAllocator_g));

            ID3D12Resource* pBackBuffer = nullptr;
            presents_stuff.buffers_ptr[i].DescriptorHandle = RTVHandle;
            g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
            g_pdevice->CreateRenderTargetView(pBackBuffer, nullptr, RTVHandle);
            presents_stuff.buffers_ptr[i].Resource = pBackBuffer;
            RTVHandle.ptr += RTVDescriptorSize;

            format = pBackBuffer->GetDesc().Format;
        }

        ImGui_ImplWin32_Init(g_window);
        ImGui_ImplDX12_Init(g_pdevice, presents_stuff.buffer_count, format, presents_stuff.DescriptorHeapImGuiRender, presents_stuff.DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(), presents_stuff.DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());
        ImGui_ImplDX12_CreateDeviceObjects();
		ImGui::StyleColorsDark();

        // Init reflex to defaults...
        _NV_SET_SLEEP_MODE_PARAMS params;
        ZeroMemory(&params, sizeof(params));
        params.version = (uint32_t)(sizeof(params) | ((1) << 16));
        params.bLowLatencyMode = reflex_params.lowlat;
        params.bLowLatencyBoost = reflex_params.boost;
        // TODO: maybe?
        params.bUseMarkersToOptimize = false;
        params.minimumIntervalUs = 0;
        auto sret = NvAPI_D3D_SetSleepMode(g_pdevice, &params);
        //spdlog::debug("NvAPI_D3D_SetSleepMode = {}", PVOID(sret));

#ifndef NDEBUG
        spdlog::debug("Present_hk::once success!");
#endif
        });
    
    // I do not want to deal with resetting all this shit, so please do not change settings for some fucking reason, ok?

    // Also a fucking meme...
    if (b_showmenu && presents_stuff.CommandQueue) {

        // Now we start rendering...
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Funny window!!!
        if (ImGui::Begin("fnuyy")) {
            ImGui::Text("Chess Battle Advanced!");
            ImGui::Separator();
            
            ImGui::Text("NVidia DLSS");
            if (!g_bDLSS)
                ImGui::BeginDisabled();
            static const char* preset_names[] = {
                "Default",
                /* 1 */ "A",
                /* 2 */ "B",
                /* 3 */ "C",
                /* 4 */ "D (Default for Quality/Balance/Performance)",
                /* 5 */ "E",
                /* 6 */ "F (Default for DLAA/UltraPerformance)",
            };
            ImGui::ListBox("Preset", &g_DLSS_preset, preset_names, sizeof(preset_names) / sizeof(preset_names[0]));
            if (!g_bDLSS)
                ImGui::EndDisabled();
            ImGui::Separator();

            ImGui::Text("NVidia Reflex (required for DLSS-G)");
            ImGui::Text("Jerking this option MIGHT crash the game with DLSS-G enabled as it depends on it!");
            if (ImGui::Checkbox("On", &reflex_params.lowlat)) {
                _NV_SET_SLEEP_MODE_PARAMS params;
                ZeroMemory(&params, sizeof(params));
                params.version = (uint32_t)(sizeof(params) | ((1) << 16));
                params.bLowLatencyMode = reflex_params.lowlat;
                // Follow the guideline...
                params.bLowLatencyBoost = reflex_params.lowlat ? reflex_params.boost : 0;
                // TODO: maybe?
                params.bUseMarkersToOptimize = false;
                params.minimumIntervalUs = 0;
                auto sret = NvAPI_D3D_SetSleepMode(g_pdevice, &params);
                spdlog::debug("NvAPI_D3D_SetSleepMode = {}", PVOID(sret));
                g_MVecs = g_Depth = nullptr;
            }
            if (!reflex_params.lowlat)
                ImGui::BeginDisabled();
            if (ImGui::Checkbox("   + Boost", &reflex_params.boost)) {
                _NV_SET_SLEEP_MODE_PARAMS params;
                ZeroMemory(&params, sizeof(params));
                params.version = (uint32_t)(sizeof(params) | ((1) << 16));
                params.bLowLatencyMode = reflex_params.lowlat;
                params.bLowLatencyBoost = reflex_params.boost;
                // TODO: maybe?
                params.bUseMarkersToOptimize = false;
                params.minimumIntervalUs = 0;
                auto sret = NvAPI_D3D_SetSleepMode(g_pdevice, &params);
                spdlog::debug("NvAPI_D3D_SetSleepMode = {}", PVOID(sret));
                g_MVecs = g_Depth = nullptr;
            }
            if (!reflex_params.lowlat)
                ImGui::EndDisabled();
            ImGui::Separator();

#ifdef DLSSG
            ImGui::Text("NVidia DLSS-G (FrameGen)");
            ImGui::Text("Jerking this option MIGHT crash the game!");
            if (!g_bDLSSG || !reflex_params.lowlat)
                ImGui::BeginDisabled();
            ImGui::Checkbox("Enabled", &g_DLSSG_enabled);
            if (!g_bDLSSG || !reflex_params.lowlat)
                ImGui::EndDisabled();
            ImGui::Separator();
#endif
        }
        ImGui::End();

        if (outputinterp) {
            ImGui::Begin("Interp");
            ImGui::Image(*(void**)&texGPUHandle, { 1280, 720 });
            ImGui::End();
        }
        
        // Now we end the frame
        ImGui::EndFrame();

        // Get correct backbuffer
        auto& CurrentFrameContext = presents_stuff.buffers_ptr[g_pSwapChain->GetCurrentBackBufferIndex()];
        CurrentFrameContext.CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER Barrier;
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = CurrentFrameContext.Resource;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        presents_stuff.CommandList->Reset(CurrentFrameContext.CommandAllocator, nullptr);
        presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
        presents_stuff.CommandList->OMSetRenderTargets(1, &CurrentFrameContext.DescriptorHandle, FALSE, nullptr);
        presents_stuff.CommandList->SetDescriptorHeaps(1, &presents_stuff.DescriptorHeapImGuiRender);

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), presents_stuff.CommandList);
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
        presents_stuff.CommandList->Close();
        presents_stuff.CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&presents_stuff.CommandList));
    }

    // TODO: better placement...
#ifdef DLSSG
    if ((g_MVecs && g_Depth) && reflex_params.lowlat && g_DLSSG_enabled && g_DLSSG_handle) {
        // Get correct backbuffer
        auto& CurrentFrameContext = presents_stuff.buffers_ptr[g_pSwapChain->GetCurrentBackBufferIndex()];
        CurrentFrameContext.CommandAllocator->Reset();

        if (!outputinterp) {
            D3D12_HEAP_PROPERTIES HeapProperties;
            ZeroMemory(&HeapProperties, sizeof(HeapProperties));
            HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
            auto desc = CurrentFrameContext.Resource->GetDesc();
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            auto hr = g_pdevice->CreateCommittedResource(
                &HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(&outputinterp)
            );
            spdlog::debug("outputinterp {} = {}", PVOID(hr), PVOID(outputinterp));

            /*
            static ID3D12DescriptorHeap* mSrvDescHeap = nullptr;

            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = 1;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            g_pdevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescHeap));

            texCPUHandle = mSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
            */

            auto cpu = presents_stuff.DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart();
            cpu.ptr += g_pdevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * presents_stuff.buffer_count;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            g_pdevice->CreateShaderResourceView(outputinterp, &srvDesc, cpu);

            texGPUHandle = presents_stuff.DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart();
            texGPUHandle.ptr += g_pdevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * presents_stuff.buffer_count;
        }


        D3D12_RESOURCE_BARRIER Barrier;
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = CurrentFrameContext.Resource;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        presents_stuff.CommandList->Reset(CurrentFrameContext.CommandAllocator, nullptr);
        presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
        presents_stuff.CommandList->OMSetRenderTargets(1, &CurrentFrameContext.DescriptorHandle, FALSE, nullptr);
        presents_stuff.CommandList->SetDescriptorHeaps(1, &presents_stuff.DescriptorHeapImGuiRender);

        auto opaque = g_Opaque;
        auto ovtbl = *(uintptr_t**)opaque;
        auto NVSDK_NGX_Parameter_SetStringResource = NVSDK_NGX_Parameter_SetD3d12Resource_fn(ovtbl[0]);
        auto NVSDK_NGX_Parameter_SetD3d12Resource = NVSDK_NGX_Parameter_SetD3d12Resource_fn(ovtbl[1]);
        auto NVSDK_NGX_Parameter_SetI = NVSDK_NGX_Parameter_SetI_fn(ovtbl[3]);
        auto NVSDK_NGX_Parameter_SetUI = NVSDK_NGX_Parameter_SetUI_fn(ovtbl[4]);
        auto NVSDK_NGX_Parameter_SetF = NVSDK_NGX_Parameter_SetF_fn(ovtbl[6]);

        // TODO: ???
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.CameraMotionIncluded", 0);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.Reset", 0);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.EnableInterp", 1);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.NotRenderingGameFrames", 0);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.DepthInverted", 1);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.MvecJittered", 0);
        // Defaults to 1 already?
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.MultiFrameIndex", 1);

        auto cd = CurrentFrameContext.Resource->GetDesc();
        auto id = ((ID3D12Resource*)g_Depth)->GetDesc();
        NVSDK_NGX_Parameter_SetI(g_Opaque, "Width", cd.Width);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "Height", cd.Height);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.InternalWidth", id.Width);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.InternalHeight", id.Height);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.run_lowres_mvec_pass", 1);
        NVSDK_NGX_Parameter_SetI(g_Opaque, "DLSSG.IsRecording", 1);
        NVSDK_NGX_Parameter_SetStringResource(g_Opaque, "DLSSG.UserDebugText", (void*)(const char*)"MRSTEYK");

        NVSDK_NGX_Parameter_SetD3d12Resource(g_Opaque, "DLSSG.Backbuffer", CurrentFrameContext.Resource);
        NVSDK_NGX_Parameter_SetD3d12Resource(g_Opaque, "DLSSG.MVecs", g_MVecs);
        NVSDK_NGX_Parameter_SetD3d12Resource(g_Opaque, "DLSSG.Depth", g_Depth);
        NVSDK_NGX_Parameter_SetD3d12Resource(g_Opaque, "DLSSG.OutputInterpolated", outputinterp);
        NVSDK_NGX_Parameter_SetD3d12Resource(g_Opaque, "DLSSG.HUDLess", nullptr);
        spdlog::debug("DLSSG {} {} {} {}", PVOID(CurrentFrameContext.Resource), g_MVecs, g_Depth, PVOID(outputinterp));

        NVSDK_NGX_Parameter_SetF(g_Opaque, "DLSSG.MvecScaleX", g_MVecsScale[0]);
        NVSDK_NGX_Parameter_SetF(g_Opaque, "DLSSG.MvecScaleY", g_MVecsScale[1]);

        static auto NVSDK_NGX_D3D12_EvaluateFeature = NVSDK_NGX_D3D12_EvaluateFeature_fn(GetProcAddress(nvngx_mod, "NVSDK_NGX_D3D12_EvaluateFeature"));
        auto ret = NVSDK_NGX_D3D12_EvaluateFeature(presents_stuff.CommandList, g_DLSSG_handle, g_Opaque, nullptr);
        spdlog::trace("Formats: {}x{} {}", cd.Width, cd.Height, +cd.Format);
        spdlog::debug("NVSDK_NGX_D3D12_EvaluateFeature(DLSSG)({}, {}, {}) = {}", PVOID(presents_stuff.CommandList), PVOID(g_DLSSG_handle), PVOID(g_Opaque), PVOID(ret));

        /*
        if (ret == 1) {
            presents_stuff.CommandList->CopyResource(CurrentFrameContext.Resource, outputinterp);
        }
        */

        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        presents_stuff.CommandList->ResourceBarrier(1, &Barrier);

        //presents_stuff.CommandList->CopyResource(CurrentFrameContext.Resource, outputinterp);
        presents_stuff.CommandList->CopyResource(CurrentFrameContext.Resource, (ID3D12Resource*)g_Depth);

        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
        presents_stuff.CommandList->Close();
        presents_stuff.CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&presents_stuff.CommandList));

        //*
        if (ret == 1) {
            // TODO: is this the best way???
            reinterpret_cast<decltype(&Present_hk)>(Present_orig)(This, SyncInterval, Flags);

            if (reflex_params.lowlat) {
                auto sret = NvAPI_D3D_Sleep(g_pdevice);
                //auto sret = 0ull;
#ifndef NDEBUG
                //spdlog::trace("NvAPI_D3D_Sleep() = {}", PVOID(sret));
#endif
            }

            //Sleep(10 / 2);

            /*
            static ID3D12Fence* fence = nullptr;
            static auto event = CreateEvent(0, 0, 0, 0);
            static int fence_val = 0;
            if (!fence) {
                g_pdevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
            }

            if (fence_val) {
                fence->SetEventOnCompletion(fence_val, event);
                //fence_val++;
                WaitForSingleObject(event, INFINITE);
            }
            */

            auto &nextframecontext = presents_stuff.buffers_ptr[g_pSwapChain->GetCurrentBackBufferIndex()];
            //CurrentFrameContext = presents_stuff.buffers_ptr[g_pSwapChain->GetCurrentBackBufferIndex()];
            nextframecontext.CommandAllocator_g->Reset();
            //*
            Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            Barrier.Transition.pResource = nextframecontext.Resource;
            Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            presents_stuff.CommandList->Reset(nextframecontext.CommandAllocator_g, nullptr);
            presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
            presents_stuff.CommandList->OMSetRenderTargets(1, &nextframecontext.DescriptorHandle, FALSE, nullptr);
            presents_stuff.CommandList->SetDescriptorHeaps(1, &presents_stuff.DescriptorHeapImGuiRender);

            presents_stuff.CommandList->CopyResource(nextframecontext.Resource, outputinterp);

            Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            presents_stuff.CommandList->ResourceBarrier(1, &Barrier);
            presents_stuff.CommandList->Close();
            //*
            presents_stuff.CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&presents_stuff.CommandList));

            //*
            //presents_stuff.CommandQueue->Signal(fence, 1);
            //fence_val = 1;

            //fence->SetEventOnCompletion(1, event);
            //fence_val++;
            //WaitForSingleObject(event, INFINITE);
            // */
        }
        //*/
    }
#endif
    g_MVecs = g_Depth = nullptr;

    // TODO: find a better place before input query...
    auto ret = reinterpret_cast<decltype(&Present_hk)>(Present_orig)(This, SyncInterval, Flags);
    if (reflex_params.lowlat) {
        auto sret = NvAPI_D3D_Sleep(g_pdevice);
        //auto sret = 0ull;
#ifndef NDEBUG
        //spdlog::trace("NvAPI_D3D_Sleep() = {}", PVOID(sret));
#endif
    }
    return ret;
}

HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd_hk(
    IDXGIFactory2* This,
    /* [annotation][in] */
    _In_  IUnknown* pDevice,
    /* [annotation][in] */
    _In_  HWND hWnd,
    /* [annotation][in] */
    _In_  const DXGI_SWAP_CHAIN_DESC1* pDesc,
    /* [annotation][in] */
    _In_opt_  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
    /* [annotation][in] */
    _In_opt_  IDXGIOutput* pRestrictToOutput,
    /* [annotation][out] */
    _COM_Outptr_  IDXGISwapChain3** ppSwapChain) {
    // Will populate ppSwapChain
    auto ret = reinterpret_cast<decltype(&CreateSwapChainForHwnd_hk)>(CreateSwapChainForHwnd_orig)(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);

#ifndef NDEBUG
    spdlog::debug(__FUNCTION__ " called!");
#endif

    // I know posses the swapchain! (and device)
    g_pSwapChain = *ppSwapChain;
    g_window = hWnd;

    auto spv = *(uintptr_t**)g_pSwapChain;
    auto present_ptr = PVOID(spv[8]);
    // Can be already created
    if (MH_CreateHook(present_ptr, &Present_hk, &Present_orig) == MH_OK) {
        MH_EnableHook(present_ptr);
    }

    //InitDLSS(g_pdevice);

#ifndef NDEBUG
    spdlog::info(__FUNCTION__ " success!");
#endif

    return ret;
}

void HookDXGIFactory2(void* factory_) {
	auto factory_vtbl = *(uintptr_t**)factory_;

	MH_CreateHook(PVOID(factory_vtbl[15]), CreateSwapChainForHwnd_hk, &CreateSwapChainForHwnd_orig);
    MH_EnableHook(PVOID(factory_vtbl[15]));

#ifndef NDEBUG
    spdlog::info(__FUNCTION__ " success!");
#endif
}

void* CreateCommandQueue_orig = nullptr;
HRESULT CreateCommandQueue_hk(
    ID3D12Device* This,
    _In_  const D3D12_COMMAND_QUEUE_DESC* pDesc,
    REFIID riid,
    _COM_Outptr_  ID3D12CommandQueue** ppCommandQueue) {
    auto ret = reinterpret_cast<decltype(&CreateCommandQueue_hk)>(CreateCommandQueue_orig)(This, pDesc, riid, ppCommandQueue);

    if (pDesc->Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        // Graphics! YAY!
#ifndef NDEBUG
        spdlog::trace(__FUNCTION__ " called for D3D12_COMMAND_LIST_TYPE_DIRECT from {}", _ReturnAddress());
#endif
        // ImGui also calls it to upload the font...
        //static std::once_flag once;
        //std::call_once(once, [&] {});
        if (!presents_stuff.CommandQueue) {
            InitDLSS(g_pdevice);
            presents_stuff.CommandQueue = *ppCommandQueue;
#ifndef NDEBUG
            spdlog::info(__FUNCTION__ " got command queue successfully!");
#endif
        }
    }

    return ret;
}

void* D3D12CreateDevice_orig = nullptr;
HRESULT D3D12CreateDevice_hk(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID* const riid, ID3D12Device** ppDevice) {
    auto ret = reinterpret_cast<decltype(&D3D12CreateDevice_hk)>(D3D12CreateDevice_orig)(pAdapter, MinimumFeatureLevel, riid, ppDevice);

    spdlog::debug(__FUNCTION__ " called!");

    // ARGH
    if (ppDevice) {
        if (*ppDevice) {
            g_pdevice = *ppDevice;
            auto vtbl = *(uintptr_t**)g_pdevice;
            auto ccq = PVOID(vtbl[8]);
            if (MH_CreateHook(ccq, &CreateCommandQueue_hk, &CreateCommandQueue_orig) == MH_OK) {
                MH_EnableHook(ccq);
            }
        }
    }

    //InitDLSS(g_pdevice);

    return ret;
}

void HookDX12() {
    auto mod = GetModuleHandleA("d3d12.dll");
    auto proc = PVOID(GetProcAddress(mod, "D3D12CreateDevice"));

    MH_CreateHook(proc, &D3D12CreateDevice_hk, &D3D12CreateDevice_orig);
    MH_EnableHook(proc);
}
