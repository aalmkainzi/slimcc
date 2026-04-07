#include "slimcc.h"

void platform_init(PPCtx *ppctx) {
  define_macro(ppctx, "__ELF__", "1");

  define_macro(ppctx, "linux", "1");
  define_macro(ppctx, "__linux", "1");
  define_macro(ppctx, "__linux__", "1");
  define_macro(ppctx, "__gnu_linux__", "1");

  init_ty_lp64(ppctx);
}

void platform_stdinc_paths(StringArray *paths) {
#if defined(__MINGW32__) || defined(__MINGW64__)
  add_include_path(paths, "C:/MinGW/include");
  add_include_path(paths, "C:/MinGW/msys/1.0/include");
  add_include_path(paths, "C:/MinGW/lib/gcc/mingw32/include");
#else
  add_include_path(paths, "/usr/local/include");
  add_include_path(paths, "/usr/include/x86_64-linux-gnu");
  add_include_path(paths, "/usr/include");
#endif
}
