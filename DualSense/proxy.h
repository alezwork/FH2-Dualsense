/* proxy.h -- how the mod gets into Azimuth on its own: it is d3d12.dll in Azimuth's folder (next to azimuth.exe).
   azimuth.exe imports Direct3D 12 by that name, and Windows looks for such a DLL in the program's folder first, so
   this one loads with the emulator, before its first line runs. Every Direct3D 12 function goes on to the real
   d3d12.dll of System32 (loaded here; its 18 names and its one ordinal-only export, with the same ordinals:
   d3d12.def), each through a jump of a few bytes. Nothing of Azimuth is changed on disk, no other mod is needed, and
   the mods that load the same way under other names (Extra Options: winmm.dll, the FH2 Mod Platform:
   d3dcompiler_47.dll) load as before, beside it. */
#pragma once

/* the real functions, in d3d12.def's order; [18]: ordinal 99 */
void *g_d3d12[19];
static const char *const kD3d12Names[18] = {
    "D3D12CoreCreateLayeredDevice", "D3D12CoreGetLayeredDeviceSize", "D3D12CoreRegisterLayers", "D3D12CreateDevice",
    "D3D12CreateRootSignatureDeserializer", "D3D12CreateVersionedRootSignatureDeserializer",
    "D3D12DeviceRemovedExtendedData", "D3D12EnableExperimentalFeatures", "D3D12GetDebugInterface", "D3D12GetInterface",
    "D3D12PIXEventsReplaceBlock", "D3D12PIXGetThreadInfo", "D3D12PIXNotifyWakeFromFenceSignal", "D3D12PIXReportCounter",
    "D3D12SerializeRootSignature", "D3D12SerializeVersionedRootSignature", "GetBehaviorValue", "SetAppCompatStringPointer"};

#define D3D12_STUB(i, name)                                                                                           \
    __attribute__((naked)) void name(void) { __asm__ volatile("jmpq *g_d3d12+" #i "*8(%rip)"); }
D3D12_STUB(0, D3D12CoreCreateLayeredDevice)
D3D12_STUB(1, D3D12CoreGetLayeredDeviceSize)
D3D12_STUB(2, D3D12CoreRegisterLayers)
D3D12_STUB(3, D3D12CreateDevice)
D3D12_STUB(4, D3D12CreateRootSignatureDeserializer)
D3D12_STUB(5, D3D12CreateVersionedRootSignatureDeserializer)
D3D12_STUB(6, D3D12DeviceRemovedExtendedData)
D3D12_STUB(7, D3D12EnableExperimentalFeatures)
D3D12_STUB(8, D3D12GetDebugInterface)
D3D12_STUB(9, D3D12GetInterface)
D3D12_STUB(10, D3D12PIXEventsReplaceBlock)
D3D12_STUB(11, D3D12PIXGetThreadInfo)
D3D12_STUB(12, D3D12PIXNotifyWakeFromFenceSignal)
D3D12_STUB(13, D3D12PIXReportCounter)
D3D12_STUB(14, D3D12SerializeRootSignature)
D3D12_STUB(15, D3D12SerializeVersionedRootSignature)
D3D12_STUB(16, GetBehaviorValue)
D3D12_STUB(17, SetAppCompatStringPointer)
D3D12_STUB(18, D3D12Ordinal99)

/* a function the real DLL does not have (an older Windows): E_NOTIMPL */
static HRESULT WINAPI d3d12_missing(void) { return (HRESULT)0x80004001L; }

static HMODULE g_d3d12Real;
static char g_d3d12Why[160];

/* the real d3d12.dll (from the process's start, inside DllMain: a system DLL) */
static void d3d12_load(void)
{
    WCHAR path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH - 16);
    int missing = 0;
    if (n && n < MAX_PATH - 16) {
        lstrcatW(path, L"\\d3d12.dll");
        g_d3d12Real = LoadLibraryW(path);
    }
    for (int i = 0; i < 19; i++) {
        FARPROC f = g_d3d12Real ? GetProcAddress(g_d3d12Real, i < 18 ? kD3d12Names[i] : MAKEINTRESOURCEA(99)) : NULL;
        g_d3d12[i] = f ? (void *)f : (void *)d3d12_missing;
        missing += !f;
    }
    if (!g_d3d12Real)
        wsprintfA(g_d3d12Why, "the real d3d12.dll did not load (error %lu): no Direct3D 12 for Azimuth", GetLastError());
    else if (missing)
        wsprintfA(g_d3d12Why, "the real d3d12.dll lacks %d of its 19 functions (an older Windows)", missing);
}
