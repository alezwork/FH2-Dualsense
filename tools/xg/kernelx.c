/* kernelx.dll -- just enough of the Xbox One kernel for the game's own xg_x.dll (the texture layout library) to run
   on a PC: its memory functions; the rest forwards to kernel32 (kernelx.def). For tools\make_ps_icons.py. */
#include <windows.h>
__declspec(dllexport) void *XMemAlloc(SIZE_T size, ULONGLONG attributes)
{
    (void)attributes;
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
}
__declspec(dllexport) void XMemFree(void *p, ULONGLONG attributes)
{
    (void)attributes;
    if (p)
        HeapFree(GetProcessHeap(), 0, p);
}
BOOL WINAPI DllMain(HINSTANCE i, DWORD r, LPVOID x) { (void)i; (void)r; (void)x; return TRUE; }
