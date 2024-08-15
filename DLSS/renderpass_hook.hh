#pragma once

void HookFSR2UpscaleRenderPass();
void UnHookFSR2UpscaleRenderPass();

extern void* g_ColorDuplicate;
extern void* g_UpscaledColor;
extern void* g_MVecs;
extern void* g_Depth;
extern float g_MVecsScale[2];