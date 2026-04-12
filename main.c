#include "slimcc.h"
#include "slimcc_lib.h"

#ifdef _WIN32

#include <io.h>

bool file_exists(char *path)
{
  return _access(path, 0) == 0;
}

#else
#include <unistd.h>

bool file_exists(char *path)
{
  return access(path, F_OK) == 0;
}

#endif


typedef enum {
  FILE_NONE = 0,
  FILE_C,
  FILE_ASM,
  FILE_PP_ASM,
  FILE_LDARG,
} FileType;

static void version(void) {
  puts("slimcc version 0.0");
}

static bool startswith(char *arg, char **p, char *str) {
  size_t len = strlen(str);
  if (!strncmp(arg, str, len)) {
    *p = arg + len;
    return true;
  }
  return false;
}

static bool take_arg(char **argv, int *i, char **arg, char *opt) {
  if (strcmp(argv[*i], opt))
    return false;
  *i += 1;
  if (argv[*i]) {
    *arg = argv[*i];
    return true;
  }
  fprintf(stderr, "missing argument to %s\n", opt);
  exit(1);
}

static bool take_arg_s(char **argv, int *i, char **p, char *str) {
  return take_arg(argv, i, p, str) || startswith(argv[*i], p, str);
}

static FileType parse_opt_x(char *s) {
  if (!strcmp(s, "c"))
    return FILE_C;
  if (!strcmp(s, "assembler"))
    return FILE_ASM;
  if (!strcmp(s, "assembler-with-cpp"))
    return FILE_PP_ASM;
  if (!strcmp(s, "none"))
    return FILE_NONE;
  error("<command line>: unknown argument for -x: %s", s);
}

static bool set_bool(char *p, bool val, char *str, bool *opt) {
  if (!strcmp(p, str)) {
    *opt = val;
    return true;
  }
  return false;
}

static bool set_true(char *p, char *str, bool *opt) {
  return set_bool(p, true, str, opt);
}

static void set_std(SlimccCtx *opts, bool is_iso, char *arg) {
  char *end;
  int val = strtoul(arg, &end, 10);

  if (end - arg == 2) {
    opts->is_iso_std = is_iso;

    switch (val) {
    case 89:
    case 90: opts->opt_std = STD_C89; return;
    case 99: opts->opt_std = STD_C99; return;
    case 11: opts->opt_std = STD_C11; return;
    case 17:
    case 18: opts->opt_std = STD_C17; return;
    case 23: opts->opt_std = STD_C23; return;
    }
  }
  error("unknown c standard");
}

static void macrochange_push(MacroChangeArr *arr, char *arg, bool is_def) {
  if (arr->len == arr->capacity) {
    arr->capacity += 4;
    arr->data = realloc(arr->data, sizeof(MacroChange) * arr->capacity);
  }
  MacroChange *m = &arr->data[arr->len++];
  m->arg = arg;
  m->is_def = is_def;
}

static void build_macros(SlimccCtx *opts, MacroChangeArr *arr, bool is_asm_pp) {
  if (is_asm_pp) {
    define_macro(opts, "__ASSEMBLER__", "1");
  } else {
    if (opts->is_iso_std)
      define_macro(opts, "__STRICT_ANSI__", "1");

    switch (opts->opt_std) {
    case STD_C99: define_macro(opts, "__STDC_VERSION__", "199901L"); break;
    case STD_C11: define_macro(opts, "__STDC_VERSION__", "201112L"); break;
    case STD_C17: define_macro(opts, "__STDC_VERSION__", "201710L"); break;
    case STD_C23: define_macro(opts, "__STDC_VERSION__", "202311L"); break;
    }
  }

  for (int i = 0; i < arr->len; i++) {
    MacroChange *m = &arr->data[i];
    if (m->is_def)
      define_macro_cli(opts, m->arg);
    else
      undef_macro(opts, m->arg);
  }
}

static char *quote_makefile(char *s) {
  char *buf = calloc(1, strlen(s) * 2 + 1);

  for (int i = 0, j = 0; s[i]; i++) {
    switch (s[i]) {
    case '$':
      buf[j++] = '$';
      buf[j++] = '$';
      break;
    case '#':
      buf[j++] = '\\';
      buf[j++] = '#';
      break;
    case ' ':
    case '\t':
      for (int k = i - 1; k >= 0 && s[k] == '\\'; k--)
        buf[j++] = '\\';
      buf[j++] = '\\';
      buf[j++] = s[i];
      break;
    default: {
      buf[j++] = s[i];
      break;
    }
    }
  }
  return buf;
}

void add_include_path(StringArray *arr, char *path) {
    size_t orig_len = strlen(path);
    size_t len = orig_len;

    while (len > 1 && path[len - 1] == '/')
      len--;

    for (int i = 0; i < arr->len; i++) {
      char *s2 = arr->data[i];
      if (!strncmp(s2, path, len) && s2[len] == '\0')
        return;
    }

    if (len != orig_len)
      path = string_dup(path, len);

    strarray_push(arr, path);
}

static void build_incl_paths(SlimccCtx *opts, char *opt_B, bool opt_nostdinc, StringArray *isystem,
                             StringArray *idirafter) {
  if (opt_B)
    add_include_path(&opts->sysincl_paths, opt_B);

  for (int i = 0; i < isystem->len; i++)
    add_include_path(&opts->sysincl_paths, isystem->data[i]);

  if (!opt_nostdinc)
    platform_stdinc_paths(&opts->sysincl_paths);

  for (int i = 0; i < idirafter->len; i++)
    add_include_path(&opts->sysincl_paths, idirafter->data[i]);

  // Filter system directories passed as -I
  for (int i = 0; i < opts->include_paths.len; i++) {
    bool match = false;
    for (int j = 0; j < opts->sysincl_paths.len; j++)
      if ((match = !strcmp(opts->sysincl_paths.data[j], opts->include_paths.data[i])))
        break;
    if (!match)
      opts->include_paths.data[opts->incl_cnt++] = opts->include_paths.data[i];
  }
  opts->include_paths.len = opts->incl_cnt;

  for (int i = 0; i < opts->sysincl_paths.len; i++)
    strarray_push(&opts->include_paths, opts->sysincl_paths.data[i]);
}

static void parse_args(SlimccCtx *opts, int argc, char **argv) {
  char *arg;
  int input_cnt = 0;
  bool has_wl = false;
  bool has_gnu_keywords_option = false;
  bool opt_nostdinc = false;
  StringArray libpaths = {0};
  StringArray isystem = {0};
  StringArray idirafter = {0};

  for (int i = 1; i < argc; i++) {
    if (*argv[i] == '\0')
      continue;

    if (*argv[i] != '-' || argv[i][1] == '\0') {
      strarray_push(&opts->input_args, argv[i]);
      input_cnt++;
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-I")) {
      add_include_path(&opts->include_paths, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-isystem")) {
      strarray_push(&isystem, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-idirafter")) {
      strarray_push(&idirafter, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-iquote")) {
      add_include_path(&opts->iquote_paths, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-D")) {
      macrochange_push(&opts->macrodefs, arg, true);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-U")) {
      macrochange_push(&opts->macrodefs, arg, false);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-imacros")) {
      strarray_push(&opts->opt_imacros, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-include")) {
      strarray_push(&opts->opt_include, arg);
      continue;
    }

    if (take_arg_s(argv, &i, &arg, "-x")) {
      strarray_push(&opts->input_args, "-x");
      strarray_push(&opts->input_args, arg);
      continue;
    }

    if (!strcmp(argv[i], "-ansi")) {
      set_std(opts, true, "89");
      continue;
    }

    if (startswith(argv[i], &arg, "-std=") ||
        startswith(argv[i], &arg, "--std=") ||
        take_arg(argv, &i, &arg, "--std")) {
      if (startswith(arg, &arg, "c"))
        set_std(opts, true, arg);
      else if (startswith(arg, &arg, "gnu"))
        set_std(opts, false, arg);
      else
        error("unknown c standard");
      continue;
    }

    if (startswith(argv[i], &arg, "-f")) {
      bool b = !startswith(arg, &arg, "no-");

      if (set_bool(arg, b, "short-enums", &opts->opt_short_enums))
        continue;

      if (set_bool(arg, b, "ms-anon-struct", &opts->opt_ms_anon_struct))
        continue;

      if (!strcmp(arg, "gnu-keywords")) {
        opts->opt_gnu_keywords = b;
        has_gnu_keywords_option = true;
        continue;
      }

      // -f only options
      if (b) {
        if (set_true(arg, "defer-ts", &opts->opt_fdefer_ts)) {
          define_macro(opts, "__STDC_DEFER_TS25755__", "2");
          continue;
        }
        if (set_bool(arg, false, "signed-char", &ty_pchar->is_unsigned) ||
            set_bool(arg, true, "unsigned-char", &ty_pchar->is_unsigned))
          continue;
      }
    }

    if (argv[i][0] == '-') {
      arg = (argv[i][1] == '-') ? &argv[i][2] : &argv[i][1];
    }

    if (!strcmp(argv[i], "-nostdinc")) {
      opt_nostdinc = true;
      continue;
    }

    if (set_bool(argv[i], true, "-Werror", &opts->opt_werror) ||
        set_bool(argv[i], false, "-Wno-error", &opts->opt_werror))
      continue;

    if (startswith(argv[i], &arg, "-W")) {
      if (strchr(arg, ','))
        error("unknown argument: %s", argv[i]);
      continue;
    }

    // These options are ignored for now.
    if (startswith(argv[i], &arg, "-march=") ||
        !strcmp(argv[i], "-fdollars-in-identifiers") ||
        !strcmp(argv[i], "-ffreestanding") ||
        !strcmp(argv[i], "-ffp-contract=off") ||
        !strcmp(argv[i], "-fno-builtin") ||
        !strcmp(argv[i], "-fno-fast-math") ||
        !strcmp(argv[i], "-fno-lto") ||
        !strcmp(argv[i], "-fno-asynchronous-unwind-tables") ||
        !strcmp(argv[i], "-fno-delete-null-pointer-checks") ||
        !strcmp(argv[i], "-fno-exceptions") ||
        !strcmp(argv[i], "-fno-omit-frame-pointer") ||
        !strcmp(argv[i], "-fno-stack-protector") ||
        !strcmp(argv[i], "-fno-strict-aliasing") ||
        !strcmp(argv[i], "-fno-strict-overflow") ||
        !strcmp(argv[i], "-fwrapv") ||
        !strcmp(argv[i], "-m64") ||
        !strcmp(argv[i], "-malign-double") ||
        !strcmp(argv[i], "-mfpmath=sse") ||
        !strcmp(argv[i], "-mno-red-zone") ||
        !strcmp(argv[i], "-msse2") ||
        !strcmp(argv[i], "-pedantic") ||
        !strcmp(argv[i], "--pedantic") ||
        !strcmp(argv[i], "-pedantic-errors") ||
        !strcmp(argv[i], "--pedantic-errors") ||
        !strcmp(argv[i], "-w"))
      continue;

    error("unknown argument: %s", argv[i]);
  }

  if (!has_gnu_keywords_option)
    opts->opt_gnu_keywords = !opts->is_iso_std;

  build_incl_paths(opts, NULL, opt_nostdinc, &isystem, &idirafter);

  bool no_input = !input_cnt && !has_wl;

  //if (no_input)
    //error("no input files");
}

static FILE *open_file(char *path) {
  if (!path || strcmp(path, "-") == 0)
    return stdout;

  FILE *out = fopen(path, "w");
  if (!out)
    error("cannot open output file: %s: %s", path, strerror(errno));
  return out;
}

static void close_file(FILE *file) {
  if (file == stdout)
    fflush(file);
  else
    fclose(file);
}

static bool endswith(char *p, char *q) {
  int len1 = strlen(p);
  int len2 = strlen(q);
  return (len1 >= len2) && !strcmp(p + len1 - len2, q);
}

static void print_linemarker(SlimccCtx *opts, FILE *out, Token *tok) {
  char *name = opts->display_files.data[tok->display_file_no];
  if (!strcmp(name, "-"))
    name = "<stdin>";
  if (!tok->at_bol)
    fprintf(out, "\n");
  fprintf(out, "# %d \"%s\"\n", tok->display_line_no, name);
}

static void print_tokens(Token *tok, FILE *out) {
  int line = 0;
  int file_no = -1;
  tok->at_bol = false;

  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (tok->kind == TK_FMARK)
      if (tok->display_file_no == tok->next->display_file_no)
        continue;

    if (tok->at_bol) {
      fprintf(out, "\n");
      line++;
    }
    if (tok->has_space)
      fprintf(out, " ");

    fprintf(out, "%.*s", tok->len, tok->loc);
  }
  fprintf(out, "\n");
}

bool in_sysincl_path(SlimccCtx *opts, int idx) {
  return idx >= opts->incl_cnt;
}

bool ignore_missing_dep(SlimccCtx *opts, char *path, char *filename, Token *tok) {
  if (!path) {
    if (tok)
      error_tok(opts, tok, "file not found");
    error("`%s` file not found", filename);
  }
  return false;
}

struct SlimccReport;

Slimcc_AST slimcc_get_ast(int argc, char *argv[], char *file_name, char *source_data, struct SlimccReport *report)
{
  SlimccCtx *sctx = calloc(1, sizeof(*sctx));
  
  {
    sctx->opt_std = STD_C23,
    sctx->scope = calloc(1, sizeof(Scope)),
    sctx->globals = calloc(1, sizeof(Obj)),
    sctx->macro_defs = calloc(1, sizeof(MacroDef));
  };
  
  init_macros(sctx);
  
  platform_init(sctx);
  parse_args(sctx, argc, (char**) argv);
  
  build_macros(sctx, &sctx->macrodefs, 0);
  Token *tok = preprocess_data(sctx, (char*) file_name, source_data, &sctx->opt_include, &sctx->opt_imacros);
  tok = prepare_parse(sctx, tok);
  
  Obj *prog = parse(sctx, tok);
  
  Slimcc_AST ret = {.objects = prog};
  
  HashMap gtags_map = sctx->scope->tags;
  Type **gtags = calloc(gtags_map.used, sizeof(*gtags));
  size_t gtags_count = 0;
  for(size_t i = 0 ; i < gtags_map.capacity ; i++)
  {
    HashEntry ent = gtags_map.buckets[i];
    if(ent.key != NULL && ent.key != (void*)-1)
    {
      gtags[gtags_count++] = ent.val;
    }
  }
  
  HashMap gvars_map = sctx->scope->vars;
  Slimcc_NamedVar *gvars = calloc(gvars_map.used, sizeof(*gvars));
  size_t gvars_count = 0;
  for(size_t i = 0 ; i < gvars_map.capacity ; i++)
  {
    HashEntry ent = gvars_map.buckets[i];
    if(ent.key != NULL && ent.key != (void*)-1)
    {
      gvars[gvars_count++] = (Slimcc_NamedVar){.var = ent.val, .name = ent.key, .name_len = ent.keylen};
    }
  }
  
  ret.gtags = gtags;
  ret.gvars = gvars;
  
  ret.n_gtags = gtags_count;
  ret.n_gvars = gvars_count;
  ret.ctx = sctx;
  
  return ret;
}

void slimcc_free_ast(Slimcc_AST *ast)
{
  SlimccCtx *sctx = ast->ctx;
  
  free(ast->gtags);
  free(ast->gvars);
  free(sctx->symbols.buckets);
  free(ast->ctx);
}