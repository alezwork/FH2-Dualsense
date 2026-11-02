/* fakeaz.c -- a stand-in for azimuth.exe to test the mod without the game (tools\test\run.cmd): it imports
   d3d12.dll (the mod) and the four file functions the mod hooks, opens the game's files through them as Azimuth
   does, keeps an icon archive in its memory as the game would, and asks the mod to switch the icons' look. Prints
   what it finds; exit code 0 = all as expected. Needs FH2_DS_TEST=1 (no controller, no game hooks). */
#include <windows.h>
#include <stdio.h>

__declspec(dllimport) HRESULT WINAPI D3D12SerializeRootSignature(const void *, int, void **, void **);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static BYTE *read_all(const WCHAR *path, DWORD *len, WCHAR *final)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *p;
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE)
        return NULL;
    if (final)
        GetFinalPathNameByHandleW(h, final, MAX_PATH, 0);
    *len = GetFileSize(h, NULL);
    p = (BYTE *)VirtualAlloc(NULL, *len + 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ReadFile(h, p, *len, &n, NULL);
    CloseHandle(h);
    return p;
}

/* two looks of an archive the same but for the spare bytes at the end of each picture (each look's CRC): the pixels
   (the .swatchbin's data but its last 16 bytes) are what the mod writes over in memory */
static int same_pictures(const BYTE *a, const BYTE *b, DWORD len)
{
    DWORD diff = 0, outside = 0;
    for (DWORD i = 0; i < len; i++) {
        if (a[i] == b[i])
            continue;
        diff++;
        /* in the 16 spare bytes before the next local header ("PK\3\4") or the central directory ("PK\1\2") */
        int spare = 0;
        for (DWORD k = i + 1; k <= i + 16 && k + 4 <= len && !spare; k++)
            spare = a[k] == 'P' && a[k + 1] == 'K' && (a[k + 2] == 3 || a[k + 2] == 1) && a[k + 3] == a[k + 2] + 1;
        outside += !spare;
    }
    printf("   %lu bytes differ, %lu of them not in the spare bytes\n", diff, outside);
    return outside == 0;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR media[MAX_PATH], path[MAX_PATH], final[MAX_PATH];
    DWORD len = 0, len2 = 0;
    if (argc < 2) {
        printf("fakeaz <game media folder>\n");
        return 2;
    }
    if (argc > 99) /* never: the import that loads the mod */
        D3D12SerializeRootSignature(NULL, 0, NULL, NULL);
    lstrcpyW(media, argv[1]);
    lstrcatW(media, L"\\");
    Sleep(2000); /* the mod's start: its thread builds the icons' search tables */
    HMODULE mod = GetModuleHandleW(L"d3d12.dll");
    typedef LONG (*LookFn)(void);
    typedef void (*SwitchFn)(int);
    LookFn look = (LookFn)(void *)GetProcAddress(mod, "Fh2IconLook");
    SwitchFn sw = (SwitchFn)(void *)GetProcAddress(mod, "Fh2SwitchIcons");
    typedef void (*ScanFn)(void);
    ScanFn scan = (ScanFn)(void *)GetProcAddress(mod, "Fh2ScanIcons");
    CHECK(look && sw && scan, "the mod is loaded and exports its icon functions");
    if (!look || !sw || !scan)
        return 1;

    /* the files: through the hooked imports */
    wsprintfW(path, L"%sUI.zip", media);
    BYTE *ui = read_all(path, &len, final);
    CHECK(ui && wcsstr(final, L"\\DualSense\\cache\\UI.zip"), "UI.zip is the mod's copy (%ls, %lu bytes)", final, len);
    wsprintfW(path, L"%szipmanifest.xml", media);
    BYTE *man = read_all(path, &len, final);
    CHECK(man && wcsstr(final, L"\\DualSense\\cache\\zipmanifest.xml"), "zipmanifest.xml is the mod's copy");
    wsprintfW(path, L"%sstripped\\stringtables\\RU.zip", media);
    BYTE *ru = read_all(path, &len, final);
    CHECK(ru && wcsstr(final, L"\\DualSense\\cache\\RU.zip"), "RU.zip is the mod's copy");
    wsprintfW(path, L"%sui\\textures\\data_bound\\Help_Buttons.zip", media);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    CHECK(GetFileAttributesExW(path, GetFileExInfoStandard, &fa), "Help_Buttons.zip attributes through the hook (%lu bytes)",
          fa.nFileSizeLow);
    wsprintfW(path, L"%sMedia\\Cars\\nothing_here.zip", media);
    CHECK(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES, "a file the mod does not give is left alone");

    LONG start = look();
    printf("look to start with: %ld\n", start);
    CHECK(start >= 0, "the icons are given in their looks");
    wsprintfW(path, L"%sui\\textures\\data_bound\\ControllerIcons.zip", media);
    BYTE *icons = read_all(path, &len, final); /* "the game's memory" */
    printf("ControllerIcons.zip from %ls\n", final);
    CHECK(icons != NULL, "ControllerIcons.zip read (%lu bytes)", len);
    scan(); /* the mod's search (in the game: on its own thread, 20 s after the icons are read) */
    int other = start == 1 ? 2 : 1;
    sw(other);
    CHECK(look() == other, "the look switched to %d", other);
    BYTE *now = read_all(path, &len2, final);
    printf("ControllerIcons.zip now from %ls\n", final);
    CHECK(now && len2 == len, "a new open gets the new look's copy, the same size");
    CHECK(now && same_pictures(icons, now, len), "the copy in memory was written over with the new look");
    sw(0);
    BYTE *x = read_all(path, &len2, final);
    CHECK(x && same_pictures(icons, x, len), "back to the Xbox look in memory too");
    printf("%s (%d failed)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
