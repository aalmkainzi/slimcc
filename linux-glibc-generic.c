#include "slimcc.h"

void platform_init_cc1(void) {
  define_macro("__ELF__", "1");

  define_macro("linux", "1");
  define_macro("__linux", "1");
  define_macro("__linux__", "1");
  define_macro("__gnu_linux__", "1");

  init_ty_lp64();
}

void platform_stdinc_paths(StringArray *paths) {
  // Replace this block with absolute path if you intend to
  // execute the compiler outside of source directory.
  // If you are thinking of just removing the error while keeping
  // the relative search path, please read:
  // https://github.com/rui314/chibicc/issues/162
  {
    char *hdr_dir = format("%s/slimcc_headers", dirname(strdup(argv0)));
    if (!file_exists(hdr_dir))
      error("can't find built-in headers");

    add_include_path(paths, format("%s/platform_fix/linux_glibc", hdr_dir));
    add_include_path(paths, format("%s/include", hdr_dir));
  }

  add_include_path(paths, "/usr/local/include");
  add_include_path(paths, "/usr/include/x86_64-linux-gnu");
  add_include_path(paths, "/usr/include");
}
