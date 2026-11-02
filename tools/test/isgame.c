/* isgame.c -- hooks.h's is_game() against game exes mapped as images: prints 1 for Forza Horizon 2, 0 for anything
   else (Horizon 3's exe has the same name). tools\test\run.cmd builds and runs it. */
#include "../../DualSense/base.h"
#include "../../DualSense/hooks.h"

int wmain(int argc, WCHAR **argv)
{
    for (int i = 1; i < argc; i++) {
        HMODULE m = LoadLibraryExW(argv[i], NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        BYTE *b = m ? (BYTE *)((ULONG_PTR)m & ~(ULONG_PTR)3) : NULL; /* the low bits mark a resource mapping */
        printf("%d %ls\n", b ? is_game(b) : -1, argv[i]);
    }
    return 0;
}
