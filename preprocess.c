#include "slimcc.h"
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <pathcch.h>  // PathCchRemoveFileSpec
#include <string.h>
#pragma comment(lib, "Pathcch.lib")

#define realpath(p, n) _fullpath(n, p, 0)

char *dirname(char *path) {
  static char buffer[MAX_PATH];
  
  if (path == NULL || *path == '\0') {
    buffer[0] = '.';
    buffer[1] = '\0';
    return buffer;
  }
  
  strncpy(buffer, path, MAX_PATH - 1);
  buffer[MAX_PATH - 1] = '\0';
  
  wchar_t wbuffer[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, MAX_PATH);
  
  HRESULT hr = PathCchRemoveFileSpec(wbuffer, MAX_PATH);
  
  if (FAILED(hr) || wbuffer[0] == '\0') {
    buffer[0] = '.';
    buffer[1] = '\0';
  } else {
    WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, MAX_PATH, NULL, NULL);
  }
  
  strncpy(path, buffer, MAX_PATH - 1);
  path[MAX_PATH - 1] = '\0';
  
  return buffer;
}
#else
#include <libgen.h>
#endif

typedef struct {
  Token *tok;
  Token *expanded;
} MacroArg;

typedef Token *macro_handler_fn(SlimccCtx*, Token*);

typedef struct Macro Macro;
struct Macro {
  Token *stop_tok;
  Macro *locked_next;
  Token *params;
  Token *body;
  macro_handler_fn *handler;
  int arg_cnt;
  bool is_objlike;
  bool is_locked;
  bool has_va_arg;
  bool align;
};

typedef struct {
  Macro *m;
  MacroArg *args;
  bool omit_comma;
} MacroContext;

static Token *preprocess3(SlimccCtx*, Token *tok);
static bool has_macro(SlimccCtx*, Token *tok);
static bool expand_macro(SlimccCtx*,Token **rest, Token *tok, bool is_root);
static Token *directives(SlimccCtx*,Token **cur, Token *start);
static Token *subst(SlimccCtx*,Token *tok, MacroContext *ctx);
static bool is_supported_attr(SlimccCtx*,Token *tok);
static char *supported_c_attr(SlimccCtx*,Token **rest, Token *tok, Token **vendor_out);
static void newline_to_space(SlimccCtx*,Token *tok);
static Token *pragma_macro(SlimccCtx*,Token *start);

static bool is_hash(Token *tok) {
  return tok->at_bol && equal(tok, "#");
}

bool is_pragma(Token **rest, Token *tok) {
  return is_hash(tok) && consume(rest, tok->next, "pragma");
}

Token *skip_line(SlimccCtx *sctx, Token *tok) {
  if (tok->at_bol)
    return tok;
  warn_tok(sctx, tok, "extra token");
  while (!tok->at_bol)
    tok = tok->next;
  return tok;
}

static Token *get_line(Token **cur, Token *tok) {
  (*cur)->next = tok;
  do {
    (*cur) = tok;
    tok = tok->next;
  } while (!tok->at_bol);
  return tok;
}

#if USE_ASAN || defined(__FILC__)
# define to_freelist(first, last) \
   do {                           \
     Token *x = first;            \
     Token *y = last->next;       \
     for (; x != y;) {            \
       Token *nxt = x->next;      \
       free(x);                   \
       x = nxt;                   \
     }                            \
   } while (0)
#else
# define to_freelist(first, last)     \
   do {                               \
     last->next = sctx->tok_freelist; \
     sctx->tok_freelist = first;      \
   } while (0)
#endif

static Token *copy_token(SlimccCtx *sctx, Token *tok) {
  Token *t;
  if ((t = sctx->tok_freelist))
    sctx->tok_freelist = t->next;
  else
    t = malloc(sizeof(Token));

  *t = *tok;
  t->alloc_next = sctx->last_alloc_tok;
  sctx->last_alloc_tok = t;
  return t;
}

static Token *new_eof(SlimccCtx *sctx, Token *tok) {
  Token *t = copy_token(sctx, tok);
  t->next = NULL;
  t->kind = TK_EOF;
  t->len = 0;
  t->at_bol = true;
  return t;
}

static Token *to_eof(Token *tok) {
  tok->kind = TK_EOF;
  tok->len = 0;
  tok->at_bol = true;
  return tok;
}

static Token *new_fmark(SlimccCtx *sctx, Token *tok) {
  Token *t = copy_token(sctx, tok);
  t->kind = TK_FMARK;
  t->len = 0;
  t->line_no = 1;
  t->at_bol = false;
  return t;
}

static Token *new_pmark(SlimccCtx *sctx, Token *tok) {
  Token *t = copy_token(sctx, tok);
  t->kind = TK_PMARK;
  t->len = 0;
  return t;
}

static void push_macro_lock(SlimccCtx *sctx, Macro *m, Token *tok) {
  m->is_locked = true;
  m->stop_tok = tok;
  m->locked_next = sctx->locked_macros;
  sctx->locked_macros = m;
}

static void pop_macro_lock(SlimccCtx *sctx, Token *tok) {
  while (sctx->locked_macros && sctx->locked_macros->stop_tok == tok) {
    sctx->locked_macros->is_locked = false;
    sctx->locked_macros = sctx->locked_macros->locked_next;
  }
}

static void pop_macro_lock_until(SlimccCtx *sctx, Token *tok, Token *end) {
  for (; tok != end; tok = tok->next)
    pop_macro_lock(sctx, tok);
}

static Token *skip_cond_incl(SlimccCtx *sctx, Token *tok) {
  Token *start = tok;
  Token *last = NULL;
  int lvl = 0;
  for (; tok->kind != TK_EOF; last = tok, tok = tok->next) {
    if (is_hash(tok)) {
      if ((equal(tok->next, "if") ||
           equal(tok->next, "ifdef") ||
           equal(tok->next, "ifndef"))) {
        lvl++;
        tok = tok->next;
        continue;
      }
      if (lvl && equal(tok->next, "endif")) {
        lvl--;
        tok = tok->next;
        continue;
      }
      if (lvl == 0 && (equal(tok->next, "endif") ||
                       equal(tok->next, "else") ||
                       equal(tok->next, "elif") ||
                       equal(tok->next, "elifdef") ||
                       equal(tok->next, "elifndef")))
        break;
    }
  }
  if (last) {
    tok->alloc_next = start->alloc_next;
    to_freelist(start, last);
  }
  return tok;
}

static Token *copy_line(SlimccCtx *sctx, Token **rest, Token *tok) {
  Token head = {0};
  Token *cur = &head;

  for (; !tok->at_bol; tok = tok->next)
    cur = cur->next = copy_token(sctx, tok);

  cur->next = new_eof(sctx, tok);
  *rest = tok;
  return head.next;
}

// Split tokens before the next newline into an EOF-terminated list.
static Token *split_line(SlimccCtx *sctx, Token **rest, Token *tok) {
  Token head = {.next = tok};
  Token *cur = &head;

  while (!cur->next->at_bol)
    cur = cur->next;

  *rest = cur->next;
  cur->next = new_eof(sctx, tok);
  return head.next;
}

static Token *split_paren2(SlimccCtx *sctx, Token **rest, Token *tok, Token *next) {
  Token *start = tok;
  Token head = {0};
  Token *cur = &head;

  int level = 0;
  while (!(level == 0 && equal(tok, ")"))) {
    if (equal(tok, "("))
      level++;
    else if (equal(tok, ")"))
      level--;
    else if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated list");

    cur = cur->next = tok;
    tok = tok->next;
  }
  *rest = tok->next;
  if (next)
    cur->next = next;
  else
    cur->next = to_eof(tok);
  return head.next;
}

static Token *split_paren(SlimccCtx *sctx, Token **rest, Token *tok) {
  return split_paren2(sctx, rest, tok, NULL);
}

static Token *split_bracket(SlimccCtx *sctx, Token **rest, Token *tok) {
  Token *start = tok;
  Token head = {0};
  Token *cur = &head;

  int level = 0;
  while (!(level == 0 && equal(tok, "]"))) {
    if (equal(tok, "["))
      level++;
    else if (equal(tok, "]"))
      level--;
    else if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated list");

    cur = cur->next = tok;
    tok = tok->next;
  }
  *rest = tok->next;
  cur->next = to_eof(tok);
  return head.next;
}

static Token *find_last_tok(SlimccCtx *sctx, Token *tok) {
  if (tok->kind == TK_EOF)
    internal_error();
  while (tok->next->kind != TK_EOF)
    tok = tok->next;
  return tok;
}

static Token *tokenize_buf(SlimccCtx *sctx, char *buf, Token *orig, Token **end) {
  if (orig->origin)
    orig = orig->origin;

  File file = *orig->file;
  file.contents = buf;
  Token *tok = tokenize(sctx, &file, NULL, end);

  for (Token *t = tok; t->kind != TK_EOF; t = t->next) {
    t->file = NULL;
    t->origin = orig;
  }
  return tok;
}

static Token *make_token(SlimccCtx *sctx, char *str, Token *orig, Token *nxt) {
  Token *tok = tokenize_buf(sctx, str, orig, NULL);
  tok->at_bol = false;
  tok->next = nxt;
  return tok;
}

static Token *new_bool_int_token(SlimccCtx *sctx, bool b, Token *orig, Token *nxt) {
  return make_token(sctx, b ? "1" : "0", orig, nxt);
}

static Token *new_num_token(SlimccCtx *sctx, int64_t val, Token *orig, Token *nxt) {
  if (val < 0)
    internal_error();
  return make_token(sctx, format("%" PRIi64 "\n", val), orig, nxt);
}

static Token *new_str_token(SlimccCtx *sctx, char *str, Token *orig) {
  size_t len = strlen(str);
  char *buf = malloc(len + 3);
  memcpy(buf + 1, str, len);
  buf[0] = buf[len + 1] = '"';
  buf[len + 2] = '\0';
  return make_token(sctx, buf, orig, orig->next);
}

static Token *to_int_token(SlimccCtx *sctx, Token *tok, int64_t val) {
  tok->kind = TK_INT_NUM;
  tok->ival = val;
  tok->ty = ty_int;
  return tok;
}

static Token *read_const_expr(SlimccCtx *sctx, Token *tok) {
  Token head = {0};
  Token *cur = &head;
  Macro *start_m = sctx->locked_macros;

  for (; tok->kind != TK_EOF; pop_macro_lock(sctx, tok)) {
    if (expand_macro(sctx, &tok, tok, false))
      continue;

    switch (tok->kind) {
    case TK_IDENT:
      if (equal(tok, "defined")) {
        Token *start = tok;
        tok = tok->next;
        bool has_paren = consume(&tok, tok, "(");

        to_int_token(sctx, start, has_macro(sctx, tok));
        cur = cur->next = start;
        tok = tok->next;
        if (has_paren)
          tok = skip(sctx, tok, ")");
        continue;
      }
      to_int_token(sctx, tok, equal(tok, "true") && sctx->opt_std >= STD_C23);
      break;
    case TK_INT_NUM:
    case TK_PP_NUM:
    case TK_PUNCT: {
      break;
    }
    default: error_tok(sctx, tok, "invalid token in preprocessor expression");
    }

    cur = cur->next = tok;
    tok = tok->next;
  }
  cur->next = tok;

  if (start_m != sctx->locked_macros)
    internal_error();
  return head.next;
}

static int64_t eval_const_expr(SlimccCtx *sctx, Token *tok) {
  Token *start = tok;
  tok = read_const_expr(sctx, tok);

  if (tok->kind == TK_EOF)
    error_tok(sctx, start, "no expression");

  arena_on(sctx, &sctx->ast_arena);
  arena_on(sctx, &sctx->node_arena);

  Token *end;
  int64_t val = const_expr(sctx, &end, tok);

  arena_off(sctx, &sctx->node_arena);
  arena_off(sctx, &sctx->ast_arena);

  if (end->kind != TK_EOF)
    error_tok(sctx, end, "extra token");
  return val;
}

static void push_cond_incl(SlimccCtx *sctx, Token *tok, bool active) {
  int idx = sctx->cond_incl.cnt++;
  if (idx >= sctx->cond_incl.capacity) {
    sctx->cond_incl.capacity = idx + 8;
    sctx->cond_incl.data = realloc(sctx->cond_incl.data, sizeof(CondIncl) * sctx->cond_incl.capacity);
  }
  sctx->cond_incl.data[idx].tok = tok;
  sctx->cond_incl.data[idx].is_else = false;
  sctx->cond_incl.data[idx].been_active = active;
}

static bool get_cond_incl(SlimccCtx *sctx, CondIncl **cond) {
  if (sctx->cond_incl.cnt <= 0)
    return false;

  *cond = &sctx->cond_incl.data[sctx->cond_incl.cnt - 1];
  return true;
}

static bool has_macro(SlimccCtx *sctx, Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(sctx, tok, "expected an identifier");
  return hashmap_get2(&sctx->macros, tok->loc, tok->len);
}

static Macro *new_macro(SlimccCtx *sctx, char *name, bool is_objlike) {
  Macro *m = arena_calloc(sctx, &sctx->pp_arena, sizeof(Macro));
  m->is_objlike = is_objlike;
  hashmap_put(&sctx->macros, name, m);
  sctx->macro_defs = sctx->macro_defs->next = arena_calloc(sctx, &sctx->pp_arena, sizeof(MacroDef));
  sctx->macro_defs->name = name;
  return m;
}

void add_macro_param(SlimccCtx *sctx, Token **cur, Token *params, Token *tok) {
  for (Token *t = params; t != (*cur)->next; t = t->next)
    if (equal_tok(t, tok))
      error_tok(sctx, tok, "duplicated macro parameter");

  (*cur) = (*cur)->next = tok;
}

static Macro *new_funclike_macro(SlimccCtx *sctx, char *name, Token **rest, Token *tok) {
  Token head = {0};
  Token *cur = &head;
  Macro *m = new_macro(sctx, name, false);

  while (!consume(rest, tok, ")")) {
    if (m->arg_cnt++)
      tok = skip(sctx, tok, ",");

    if (tok->kind != TK_IDENT) {
      *rest = skip(sctx, skip(sctx, tok, "..."), ")");
      static Token va_args = {.loc = "__VA_ARGS__", .len = 11};
      add_macro_param(sctx, &cur, head.next, &va_args);
      m->has_va_arg = true;
      break;
    }
    if (equal(tok->next, "...")) {
      *rest = skip(sctx, tok->next->next, ")");
      add_macro_param(sctx, &cur, head.next, tok);
      m->has_va_arg = true;
      break;
    }
    add_macro_param(sctx, &cur, head.next, tok);
    tok = tok->next;
  }
  cur->next = NULL;
  m->params = head.next;
  return m;
}

static Macro *read_macro_name(SlimccCtx *sctx, Token **rest, Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(sctx, tok, "macro name must be an identifier");
  char *name = string_dup(tok->loc, tok->len);
  tok = tok->next;

  Macro *m;
  if (!tok->has_space && equal(tok, "("))
    m = new_funclike_macro(sctx, name, &tok, tok->next);
  else
    m = new_macro(sctx, name, true);
  *rest = tok;
  return m;
}

static void read_macro_definition(SlimccCtx *sctx, Token **rest, Token *tok) {
  Macro *m = read_macro_name(sctx, &tok, tok);
  m->body = split_line(sctx, rest, tok);
}

static void read_macro_definition2(SlimccCtx *sctx, Token **rest, Token *tok) {
  Token *start = tok;
  Macro *m = read_macro_name(sctx, &tok, tok);
  tok = skip_line(sctx, tok);

  Token head = {0};
  Token *cur = &head;
  for (;; tok = tok->next) {
    if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated list");
    if (is_hash(tok) && equal(tok->next, "enddef"))
      break;
    cur = cur->next = tok;
    newline_to_space(sctx, cur);
  }
  cur->next = new_eof(sctx, start);
  m->body = head.next;
  *rest = skip_line(sctx, tok->next->next);
}

static Token *read_macro_arg_one(SlimccCtx *sctx, Token **rest, Token *tok, bool read_rest) {
  Token head = {0};
  Token *cur = &head;
  int level = 0;
  Token *start = tok;

  for (;;) {
    if (level == 0 && equal(tok, ")"))
      break;
    if (level == 0 && !read_rest && equal(tok, ","))
      break;

    if (equal(tok, "("))
      level++;
    else if (equal(tok, ")"))
      level--;

    if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated list");

    cur = cur->next = copy_token(sctx, tok);
    tok = tok->next;
  }
  cur->next = new_eof(sctx, tok);
  *rest = tok;
  return head.next;
}

static MacroContext read_macro_args(SlimccCtx *sctx, Token *tok, Macro *m) {
  MacroContext ctx = {.m = m};
  ctx.args = calloc(m->arg_cnt, sizeof(MacroArg));

  for (int idx = 0; idx < m->arg_cnt; idx++) {
    bool is_va_arg = m->has_va_arg && (idx == m->arg_cnt - 1);

    MacroArg *ap = &ctx.args[idx];
    if (is_va_arg && equal(tok, ")"))
      ctx.omit_comma = !(sctx->is_iso_std && idx == 0);
    else if (idx)
      tok = skip(sctx, tok, ",");

    ap->tok = read_macro_arg_one(sctx, &tok, tok, is_va_arg);
  }

  skip(sctx, tok, ")");
  return ctx;
}

static Token *expand_tok(SlimccCtx *sctx, Token *tok) {
  Token head = {0};
  Token *cur = &head;
  Macro *start_m = sctx->locked_macros;

  for (; tok->kind != TK_EOF; pop_macro_lock(sctx, tok)) {
    if (expand_macro(sctx, &tok, tok, false))
      continue;

    cur = cur->next = copy_token(sctx, tok);
    tok = tok->next;
  }
  cur->next = new_eof(sctx, tok);

  if (start_m != sctx->locked_macros)
    internal_error();
  return head.next;
}

static Token *expand_arg(SlimccCtx *sctx, MacroArg *arg) {
  if (arg->expanded)
    return arg->expanded;

  return arg->expanded = expand_tok(sctx, arg->tok);
}

static bool has_non_empty_va_arg(SlimccCtx *sctx, MacroContext *ctx, MacroArg **arg_p) {
  if (ctx->m->has_va_arg) {
    MacroArg *va = &ctx->args[ctx->m->arg_cnt - 1];
    if (arg_p)
      *arg_p = va;
    return expand_arg(sctx, va)->kind != TK_EOF;
  }
  return false;
}

static MacroArg *find_arg(SlimccCtx *sctx, Token **rest, Token *tok, MacroContext *ctx) {
  if (tok->kind != TK_IDENT)
    return NULL;

  int idx = 0;
  for (Token *t = ctx->m->params; t; t = t->next) {
    if (equal_tok(t, tok)) {
      if (rest)
        *rest = tok->next;
      return &ctx->args[idx];
    }
    idx++;
  }

  if (equal(tok, "__VA_OPT__") && equal(tok->next, "(")) {
    MacroArg *arg = arena_malloc(sctx, &sctx->pp_arena, sizeof(MacroArg));
    arg->tok = read_macro_arg_one(sctx, &tok, tok->next->next, true);

    if (has_non_empty_va_arg(sctx, ctx, NULL))
      arg->tok = subst(sctx, arg->tok, ctx);
    else
      arg->tok = new_eof(sctx, tok);

    arg->expanded = arg->tok;
    if (rest)
      *rest = tok->next;
    return arg;
  }
  return NULL;
}

// Concatenates all tokens in `tok` and returns a new string.
static char *join_tokens(SlimccCtx *sctx, Token *tok, Token *end, bool add_slash) {
  // Compute the length of the resulting token.
  int len = 1;
  for (Token *t = tok; t != end; t = t->next) {
    if (t->has_space && len != 1)
      len++;

    if (add_slash && (t->kind == TK_INT_NUM || t->kind == TK_STR || t->kind == TK_ASM_STR))
      for (int i = 0; i < t->len; i++)
        if (t->loc[i] == '\\' || t->loc[i] == '"')
          len++;

    len += t->len;
  }

  char *buf = calloc(1, len);

  // Copy token texts.
  int pos = 0;
  for (Token *t = tok; t != end; t = t->next) {
    if (t->has_space && pos != 0)
      buf[pos++] = ' ';

    if (add_slash && (t->kind == TK_INT_NUM || t->kind == TK_STR || t->kind == TK_ASM_STR)) {
      for (int i = 0; i < t->len; i++) {
        if (t->loc[i] == '\\' || t->loc[i] == '"')
          buf[pos++] = '\\';
        buf[pos++] = t->loc[i];
      }
      continue;
    }

    memcpy(buf + pos, t->loc, t->len);
    pos += t->len;
  }
  buf[pos] = '\0';
  return buf;
}

static Token *stringize(SlimccCtx *sctx, Token *hash, Token *tok) {
  Token head = {0};
  Token *cur = &head;
  for (; tok->kind != TK_EOF; tok = tok->next)
    if (tok->kind != TK_PMARK)
      cur = cur->next = tok;
  cur->next = tok;

  return new_str_token(sctx, join_tokens(sctx, head.next, tok, true), hash);
}

static void align_token(SlimccCtx *sctx, Token *tok1, Token *tok2) {
  tok1->at_bol = tok2->at_bol;
  tok1->has_space = tok2->has_space;
}

static void newline_to_space(SlimccCtx *sctx, Token *tok) {
  if (tok->at_bol) {
    tok->at_bol = false;
    tok->has_space = true;
  }
}

// Concatenate two tokens to create a new token.
static Token *paste(SlimccCtx *sctx, Token *lhs, Token *rhs) {
  char *buf = format("%.*s%.*s", lhs->len, lhs->loc, rhs->len, rhs->loc);

  Token *tok = tokenize_buf(sctx, buf, lhs, NULL);
  align_token(sctx, tok, lhs);

  if (tok->next->kind != TK_EOF) {
    error_tok(sctx, lhs, "pasting forms '%s', an invalid token", buf);
  }
  if (tok->origin == lhs)
    tok->origin = copy_token(sctx, lhs);

  Token *t = lhs->alloc_next;
  *lhs = *tok;
  lhs->alloc_next = t;
  return lhs;
}

// Replace func-like macro parameters with given arguments.
static Token *subst(SlimccCtx *sctx, Token *tok, MacroContext *ctx) {
  Token head = {0};
  Token *cur = &head;

  while (tok->kind != TK_EOF) {
    Token *start = tok;

    if (equal(tok, "#")) {
      MacroArg *arg = find_arg(sctx, &tok, tok->next, ctx);
      if (!arg)
        error_tok(sctx, tok->next, "'#' is not followed by a macro parameter");
      cur = cur->next = stringize(sctx, start, arg->tok);
      align_token(sctx, cur, start);
      continue;
    }

    if (equal(tok, ",") && equal(tok->next, "##") && ctx->m->has_va_arg) {
      MacroArg *arg = find_arg(sctx, NULL, tok->next->next, ctx);
      if (arg && (arg == &ctx->args[ctx->m->arg_cnt - 1])) {
        if (ctx->omit_comma) {
          tok = tok->next->next->next;
          continue;
        }
        cur = cur->next = copy_token(sctx, tok);
        tok = tok->next->next;
        continue;
      }
    }

    if (equal(tok, "##")) {
      if (cur == &head)
        error_tok(sctx, tok, "'##' cannot appear at start of macro expansion");

      if (tok->next->kind == TK_EOF)
        error_tok(sctx, tok, "'##' cannot appear at end of macro expansion");

      if (cur->kind == TK_PMARK) {
        tok = tok->next;
        continue;
      }

      MacroArg *arg = find_arg(sctx, &tok, tok->next, ctx);
      if (arg) {
        if (arg->tok->kind == TK_EOF)
          continue;

        if (arg->tok->kind != TK_PMARK)
          cur = paste(sctx, cur, arg->tok);

        for (Token *t = arg->tok->next; t->kind != TK_EOF; t = t->next)
          cur = cur->next = copy_token(sctx, t);
        continue;
      }
      cur = paste(sctx, cur, tok->next);
      tok = tok->next->next;
      continue;
    }

    MacroArg *arg = find_arg(sctx, &tok, tok, ctx);
    if (arg) {
      Token *t = equal(tok, "##") ? arg->tok : expand_arg(sctx, arg);

      if (t->kind == TK_EOF) {
        cur = cur->next = new_pmark(sctx, t);
        align_token(sctx, cur, start);
        continue;
      }
      cur = cur->next = copy_token(sctx, t);
      align_token(sctx, cur, start);

      while ((t = t->next)->kind != TK_EOF)
        cur = cur->next = copy_token(sctx, t);
      continue;
    }

    if (equal(tok, "__VA_TAIL__") && consume(&tok, tok->next, "(")) {
      Macro *tail_m = NULL;
      Token *rparen = NULL;
      if (equal(tok, ")")) {
        tail_m = ctx->m;
        rparen = tok;
        tok = tok->next;
      } else if (tok->kind == TK_IDENT) {
        tail_m = hashmap_get2(&sctx->macros, tok->loc, tok->len);
        rparen = tok->next;
        tok = skip(sctx, tok->next, ")");
      }
      if (!(tail_m && tail_m->arg_cnt))
        error_tok(sctx, start,
                  "expected function-like macro with at least one named parameter");

      MacroArg *vaarg;
      if (!has_non_empty_va_arg(sctx, ctx, &vaarg)) {
        cur = cur->next = new_pmark(sctx, tok);
        continue;
      }
      Token *tail_arg_tok = copy_line(sctx, &(Token *){0}, vaarg->expanded);
      find_last_tok(sctx, tail_arg_tok)->next = rparen;

      MacroContext tail_ctx = read_macro_args(sctx, tail_arg_tok, tail_m);
      for (int i = 0; i < tail_m->arg_cnt; i++)
        tail_ctx.args[i].expanded = tail_ctx.args[i].tok;

      cur->next = subst(sctx, tail_m->body, &tail_ctx);
      free(tail_ctx.args);
      cur = find_last_tok(sctx, cur);
      continue;
    }

    cur = cur->next = copy_token(sctx, tok);
    tok = tok->next;
    continue;
  }

  cur->next = tok;
  return head.next;
}

static Token *insert_objlike(SlimccCtx *sctx, Token *tok, Token *stop_tok, Token *orig) {
  Token head = {0};
  Token *cur = &head;
  if (orig->origin)
    orig = orig->origin;

  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (equal(tok, "##")) {
      if (cur == &head || tok->next->kind == TK_EOF)
        error_tok(sctx, tok, "'##' cannot appear at either end of macro expansion");

      tok = tok->next;
      paste(sctx, cur, tok);
    } else {
      cur = cur->next = copy_token(sctx, tok);
    }
    cur->origin = orig;
    cur->is_root = true;
  }
  cur->next = stop_tok;
  return head.next;
}

static Token *insert_funclike(SlimccCtx *sctx, Token *tok, Token *stop_tok, Token *orig) {
  Token head = {0};
  Token *cur = &head;
  if (orig->origin)
    orig = orig->origin;

  bool space = false;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (tok->kind == TK_PMARK) {
      space |= tok->has_space;
      continue;
    }
    cur = cur->next = tok;
    cur->origin = orig;
    cur->is_root = true;
    cur->has_space |= space;
    space = false;
  }
  cur->next = stop_tok;
  return head.next;
}

static Token *prepare_funclike_args(SlimccCtx *sctx, Token *start) {
  pop_macro_lock(sctx, start);

  Token *cur = start;
  int lvl = 0;
  for (Token *tok = start->next;;) {
    if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated list");

    if (!sctx->locked_macros) {
      if (is_hash(tok)) {
        tok = directives(sctx, &cur, tok);
        continue;
      }
    } else {
      pop_macro_lock(sctx, tok);
      if (tok->kind == TK_IDENT) {
        Macro *m = hashmap_get2(&sctx->macros, tok->loc, tok->len);
        if (m && m->is_locked)
          tok->dont_expand = true;
      }
    }

    cur = cur->next = tok;
    newline_to_space(sctx, cur);

    if (lvl == 0 && equal(tok, ")"))
      break;

    if (equal(tok, "("))
      lvl++;
    else if (equal(tok, ")"))
      lvl--;

    tok = tok->next;
  }
  return cur->next;
}

static void free_funclike_args(SlimccCtx *sctx, Token *tok, Token *stop_tok) {
  while (tok != stop_tok) {
    Token *start = tok;
    Token *last = NULL;
    while (tok != stop_tok && tok->next->alloc_next == tok) {
      last = tok;
      tok = tok->next;
    }
    if (last) {
      tok->alloc_next = start->alloc_next;
      to_freelist(start, last);
      continue;
    }
    tok = tok->next;
  }
}

static bool expand_macro(SlimccCtx *sctx, Token **rest, Token *tok, bool is_root) {
  if (tok->kind != TK_IDENT || tok->dont_expand)
    return false;

  Macro *m = hashmap_get2(&sctx->macros, tok->loc, tok->len);
  if (!m)
    return false;

  if (m->is_locked) {
    tok->dont_expand = true;
    return false;
  }

  // Built-in dynamic macro application such as __LINE__
  if (m->handler) {
    if (m->handler == &pragma_macro && !is_root)
      return false;

    *rest = m->handler(sctx, tok);
    if (m->align)
      align_token(sctx, *rest, tok);
    return true;
  }

  // If a funclike macro token is not followed by an argument list,
  // treat it as a normal identifier.
  if (!m->is_objlike && !equal(tok->next, "("))
    return false;

  if (!m->is_objlike && m->body->kind == TK_EOF && equal(tok, "__attribute__")) {
    char *slash = strrchr(m->body->file->name, '/');
    if (slash && !strcmp(slash + 1, "cdefs.h")) {
      push_macro_lock(sctx, m, prepare_funclike_args(sctx, tok->next));
      return true;
    }
  }

  Token *stop_tok, *free_alloc_end;

  if (m->is_objlike) {
    stop_tok = tok->next;
    free_alloc_end = sctx->last_alloc_tok;

    *rest = insert_objlike(sctx, m->body, stop_tok, tok);
  } else {
    stop_tok = prepare_funclike_args(sctx, tok->next);
    free_alloc_end = sctx->last_alloc_tok;

    MacroContext ctx = read_macro_args(sctx, tok->next->next, m);

    if (is_root)
      free_funclike_args(sctx, tok->next, stop_tok);

    *rest = insert_funclike(sctx, subst(sctx, m->body, &ctx), stop_tok, tok);
    free(ctx.args);
  }

  if (*rest != stop_tok) {
    push_macro_lock(sctx, m, stop_tok);
    align_token(sctx, *rest, tok);
  } else {
    (*rest)->at_bol |= tok->at_bol;
    (*rest)->has_space |= tok->has_space;
  }

  {
    Token head = {0};
    Token *cur = &head;

    for (Token *t = sctx->last_alloc_tok; t != free_alloc_end;) {
      if (t->is_root) {
        t->is_root = false;
        cur = cur->alloc_next = t;
        t = t->alloc_next;
        continue;
      }
      Token *nxt = t->alloc_next;
      to_freelist(t, t);
      t = nxt;
    }
    cur->alloc_next = free_alloc_end;
    sctx->last_alloc_tok = head.alloc_next;
  }
  return true;
}

static char *search_include_paths2(SlimccCtx *sctx, char *filename, char *dir, InclIdx *idx) {
  if (filename[0] == '/') {
    *idx = INCL_ABS;
    return filename;
  }

  if (*idx <= INCL_REL) {
    if (dir) {
      dir = strdup(dir);
      char *path = format("%s/%s", dirname(dir), filename);
      if (file_exists(path)) {
        *idx = INCL_REL;
        free(dir);
        return path;
      }
      free(path);
      free(dir);

      for (int i = 0; i < sctx->iquote_paths.len; i++) {
        char *path = format("%s/%s", sctx->iquote_paths.data[i], filename);
        if (file_exists(path)) {
          *idx = INCL_REL;
          return path;
        }
        free(path);
      }
    }
    *idx = 0;
  }

  for (; *idx < sctx->include_paths.len; (*idx)++) {
    char *path = format("%s/%s", sctx->include_paths.data[*idx], filename);
    if (file_exists(path))
      return path;
    free(path);
  }
  return NULL;
}

static char *search_include_paths(SlimccCtx *sctx, char *filename, char *dir) {
  return search_include_paths2(sctx, filename, dir, &(InclIdx){INCL_REL});
}

static char *read_filename(SlimccCtx *sctx, Token **rest, Token *tok, char **dir) {
  // Pattern 3: #include FOO
  // In this case FOO must be macro-expanded to either
  // a single string token or a sequence of "<" ... ">".
  bool is_expanded = false;
  if (tok->kind == TK_IDENT) {
    tok = expand_tok(sctx, tok);
    is_expanded = true;
  }

  char *filename = NULL;
  if (tok->kind == TK_STR || tok->kind == TK_ASM_STR) {
    // Pattern 1: #include "foo.h"
    // A double-quoted filename for #include is a special kind of
    // token, and we don't want to interpret any escape sequences in it.
    // For example, "\f" in "C:\foo" is not a formfeed character but
    // just two non-control characters, backslash and f.
    // So we don't want to use token->str.
    filename = string_dup(tok->loc + 1, tok->len - 2);
    *dir = (tok->origin ? tok->origin : tok)->file->name;
  } else if (equal(tok, "<")) {
    // Pattern 2: #include <foo.h>
    // Reconstruct a filename from between "<" and ">".
    Token *start = tok;

    // Find closing ">".
    for (; !equal(tok, ">"); tok = tok->next)
      if (tok->kind == TK_EOF)
        error_tok(sctx, tok, "expected '>'");

    if (!is_expanded && start->file == tok->file && start->loc < tok->loc)
      filename = string_dup(start->loc + 1, tok->loc - start->loc - 1);
    else
      filename = join_tokens(sctx, start->next, tok, false);
  }

  if (filename && *filename != '\0') {
    if (rest)
      *rest = tok->next;
    else
      skip_line(sctx, tok->next);
    return filename;
  }
  error_tok(sctx, tok, "expected a filename");
}

static char *read_include_filename(SlimccCtx *sctx, Token *tok, char **dir) {
  return read_filename(sctx, NULL, tok, dir);
}

static Token *include_file(SlimccCtx *sctx, Token *tok, char *path, Token *filename_tok, InclIdx idx) {
  char *rp = realpath(path, NULL);
  if (hashmap_get(&sctx->pragma_once, rp))
  {
    free(rp);
    return tok;
  }
  free(rp);

  char *guard_name = hashmap_get(&sctx->include_guards, path);
  if (guard_name && hashmap_get(&sctx->macros, guard_name))
    return tok;

  Token *end = NULL;
  Token *start = tokenize_file(sctx, path, filename_tok, &end);
  start->file->incl_idx = idx;
  start->file->is_syshdr = filename_tok->file->is_syshdr || in_sysincl_path(sctx, idx);

  Token *fmark = NULL;

  if (!end) {
    if (fmark) {
      fmark->next = tok;
      return fmark;
    }
    return tok;
  }

  if (is_hash(start) &&
      equal(start->next, "ifndef") &&
      start->next->next->kind == TK_IDENT &&
      equal(end, "endif"))
    start->next->is_incl_guard = end->is_incl_guard = true;

  end->next = tok;

  if (fmark) {
    fmark->next = start;
    return fmark;
  }
  return start;
}

static Token *embed_file(SlimccCtx *sctx, Token *cont, Token *tok, char *path, Token *start) {
  Token *limit_seq = NULL;
  Token *if_empty_seq = NULL;
  Token *prefix_seq = NULL;
  Token *suffix_seq = NULL;

  for (;;) {
    if (equal(tok, "limit"))
      tok = skip_paren(sctx, limit_seq = skip(sctx, tok->next, "("));
    else if (equal(tok, "if_empty"))
      tok = skip_paren(sctx, if_empty_seq = skip(sctx, tok->next, "("));
    else if (equal(tok, "prefix"))
      tok = skip_paren(sctx, prefix_seq = skip(sctx, tok->next, "("));
    else if (equal(tok, "suffix"))
      tok = skip_paren(sctx, suffix_seq = skip(sctx, tok->next, "("));
    else
      break;
  }
  Token *dummy;
  int64_t limit = 0;
  if (limit_seq)
    limit = eval_const_expr(sctx, split_paren2(sctx, &dummy, limit_seq, NULL));

  if (!cont) {
    enum { EMBED_NOT_FOUND = 0, EMBED_FOUND = 1, EMBED_EMPTY = 2 };

    if (tok->kind != TK_EOF)
      return to_int_token(sctx, start, EMBED_NOT_FOUND);

    FILE *fp;
    if (!path || !(fp = fopen(path, "r")))
      return to_int_token(sctx, start, EMBED_NOT_FOUND);
    bool is_empty = !fread(&(char){0}, 1, sizeof(char), fp);
    fclose(fp);
    if (is_empty || (limit_seq && limit == 0))
      return to_int_token(sctx, start, EMBED_EMPTY);
    return to_int_token(sctx, start, EMBED_FOUND);
  }
  if (tok->kind != TK_EOF)
    error_tok(sctx, start, "unknown embed parameter");

  FILE *fp;
  if (!path || !(fp = fopen(path, "r")))
    error_tok(sctx, start, "%s: cannot open file: %s", path, strerror(errno));

  Token head = {0};
  Token *cur = &head;
  for (; !limit_seq || limit > 0; limit--) {
    unsigned char buf;
    if (!fread(&buf, 1, sizeof(buf), fp))
      break;

    if (cur != &head)
      cur = cur->next = make_token(sctx, ",", start, NULL);

    cur = cur->next = new_num_token(sctx, buf, start, NULL);
  }
  fclose(fp);
  if (cur == &head) {
    if (if_empty_seq)
      return split_paren2(sctx, &dummy, if_empty_seq, cont);
    return cont;
  }
  if (prefix_seq)
    head.next = split_paren2(sctx, &dummy, prefix_seq, head.next);
  if (suffix_seq)
    cur->next = split_paren2(sctx, &dummy, suffix_seq, cont);
  else
    cur->next = cont;
  return head.next;
}

// Read #line arguments
static void read_line_marker(SlimccCtx *sctx, Token **rest, Token *tok) {
  Token *start = tok;
  tok = expand_tok(sctx, split_line(sctx, rest, tok));

  Node node = {.tok = start};
  convert_pp_number(sctx, tok, &node);
  if (node.ty->kind != TY_INT)
    error_tok(sctx, tok, "invalid line marker");

  start->file->line_delta = node.num.val - start->line_no - 1;

  tok = tok->next;
  if (tok->kind == TK_EOF)
    return;

  if (tok->kind != TK_STR && tok->kind != TK_ASM_STR)
    error_tok(sctx, tok, "filename expected");

  start->file->display_file_no = add_display_file(sctx, tok->str);

  if (tok->next->kind != TK_EOF)
    error_tok(sctx, tok->next, "unknown line directive form");
}

static void finalize_tok2(SlimccCtx *sctx, Token *tok, Token *orig) {
  tok->display_file_no = orig->file->display_file_no;
  tok->display_line_no = orig->file->line_delta + orig->line_no + orig->display_line_no;
  tok->is_root = true;
}

static void finalize_tok(SlimccCtx *sctx, Token *tok) {
  Token *orig;
  if (tok->origin) {
    orig = tok->origin;
    orig->is_root = true;
  } else {
    orig = tok;
  }
  finalize_tok2(sctx, tok, orig);
}

void preprocess2(SlimccCtx *sctx, Token *tok, Token **cur) {
  Macro *start_m = sctx->locked_macros;

  for (; tok->kind != TK_EOF; pop_macro_lock(sctx, tok)) {
    if (expand_macro(sctx, &tok, tok, true))
      continue;

    if (is_hash(tok) && !sctx->locked_macros) {
      tok = directives(sctx, cur, tok);
      continue;
    }

    finalize_tok(sctx, tok);

    (*cur) = (*cur)->next = tok;
    tok = tok->next;
  }
  (*cur)->next = tok;

  CondIncl *cond;
  if (get_cond_incl(sctx, &cond))
    error_tok(sctx, cond->tok, "unterminated conditional directive");

  if (start_m != sctx->locked_macros)
    internal_error();
}

static Token *pass_line(SlimccCtx *sctx, Token **cur, Token *tok) {
  Token *start = tok;
  tok = get_line(cur, start);

  for (Token *t = start; t != tok; t = t->next)
    finalize_tok(sctx, t);

  return tok;
}

static Token *directives(SlimccCtx *sctx, Token **cur, Token *start) {
  Token *tok = start->next;

  if (equal(tok, "embed")) {
    Token *file_tok = tok->next;
    Token *cont;
    char *dir = NULL;
    char *filename = read_filename(sctx, &tok, split_line(sctx, &cont, file_tok), &dir);
    char *path = search_include_paths(sctx, filename, dir);
    if (ignore_missing_dep(sctx, path, filename, file_tok))
      return cont;
    return embed_file(sctx, cont, tok, path, file_tok);
  }

  if (equal(tok, "include")) {
    Token *file_tok = tok->next;
    char *dir = NULL;
    char *filename = read_include_filename(sctx, split_line(sctx, &tok, file_tok), &dir);

    InclIdx idx = INCL_REL;
    char *path = search_include_paths2(sctx, filename, dir, &idx);
    if (ignore_missing_dep(sctx, path, filename, file_tok))
      return tok;
    return include_file(sctx, tok, path, file_tok, idx);
  }

  if (equal(tok, "include_next")) {
    Token *file_tok = tok->next;
    char *dir = NULL;
    char *filename = read_include_filename(sctx, split_line(sctx, &tok, file_tok), &dir);

    InclIdx idx = file_tok->file->incl_idx + 1;
    char *path = search_include_paths2(sctx, filename, dir, &idx);
    if (ignore_missing_dep(sctx, path, filename, file_tok))
      return tok;
    return include_file(sctx, tok, path, file_tok, idx);
  }

  if (equal(tok, "define")) {
    read_macro_definition(sctx, &tok, tok->next);
    return tok;
  }

  if (equal(tok, "def")) {
    read_macro_definition2(sctx, &tok, tok->next);
    return tok;
  }

  if (equal(tok, "undef")) {
    tok = tok->next;
    if (tok->kind != TK_IDENT)
      error_tok(sctx, tok, "macro name must be an identifier");
    undef_macro(sctx, string_dup(tok->loc, tok->len));
    return skip_line(sctx, tok->next);
  }

  if (equal(tok, "if")) {
    bool active = eval_const_expr(sctx, split_line(sctx, &tok, tok->next));
    push_cond_incl(sctx, start, active);
    if (!active)
      return skip_cond_incl(sctx, tok);
    return tok;
  }

  if (equal(tok, "ifdef")) {
    bool active = has_macro(sctx, tok->next);
    push_cond_incl(sctx, tok, active);
    tok = skip_line(sctx, tok->next->next);
    if (!active)
      return skip_cond_incl(sctx, tok);
    return tok;
  }

  if (equal(tok, "ifndef")) {
    bool active = !has_macro(sctx, tok->next);
    push_cond_incl(sctx, tok, active);
    tok = skip_line(sctx, tok->next->next);
    if (!active)
      return skip_cond_incl(sctx, tok);
    return tok;
  }

  if (equal(tok, "elif")) {
    CondIncl *cond;
    if (!get_cond_incl(sctx, &cond) || cond->is_else)
      error_tok(sctx, start, "stray #elif");
    cond->tok->is_incl_guard = false;

    if (!cond->been_active && eval_const_expr(sctx, split_line(sctx, &tok, tok->next))) {
      cond->been_active = true;
      return tok;
    }
    return skip_cond_incl(sctx, tok);
  }

  if (equal(tok, "elifdef")) {
    CondIncl *cond;
    if (!get_cond_incl(sctx, &cond) || cond->is_else)
      error_tok(sctx, start, "stray #elifdef");
    cond->tok->is_incl_guard = false;

    if (!cond->been_active && has_macro(sctx, tok->next)) {
      cond->been_active = true;
      return skip_line(sctx, tok->next->next);
    }
    return skip_cond_incl(sctx, tok);
  }

  if (equal(tok, "elifndef")) {
    CondIncl *cond;
    if (!get_cond_incl(sctx, &cond) || cond->is_else)
      error_tok(sctx, start, "stray #elifndef");
    cond->tok->is_incl_guard = false;

    if (!cond->been_active && !has_macro(sctx, tok->next)) {
      cond->been_active = true;
      return skip_line(sctx, tok->next->next);
    }
    return skip_cond_incl(sctx, tok);
  }

  if (equal(tok, "else")) {
    CondIncl *cond;
    if (!get_cond_incl(sctx, &cond) || cond->is_else)
      error_tok(sctx, start, "stray #else");
    cond->tok->is_incl_guard = false;
    cond->is_else = true;

    tok = skip_line(sctx, tok->next);

    if (cond->been_active)
      return skip_cond_incl(sctx, tok);
    return tok;
  }

  if (equal(tok, "endif")) {
    CondIncl *cond;
    if (!get_cond_incl(sctx, &cond))
      error_tok(sctx, start, "stray #endif");

    if (tok->is_incl_guard && cond->tok->is_incl_guard && tok->file == cond->tok->file) {
      Token *name_tok = cond->tok->next;
      char *guard_name = string_dup(name_tok->loc, name_tok->len);
      hashmap_put(&sctx->include_guards, tok->file->name, guard_name);
    }

    sctx->cond_incl.cnt--;
    return skip_line(sctx, tok->next);
  }

  if (equal(tok, "line")) {
    read_line_marker(sctx, &tok, tok->next);
    return tok;
  }

  if (tok->kind == TK_PP_NUM) {
    read_line_marker(sctx, &tok, tok);
    return tok;
  }

  if (equal(tok, "error"))
    error_tok(sctx, tok, "error");

  if (equal(tok, "warning")) {
    warn_tok(sctx, tok, "warning");
    do {
      tok = tok->next;
    } while (!tok->at_bol);
    return tok;
  }

  if (equal(tok, "pragma")) {
    if (equal(tok->next, "once")) {
      hashmap_put(&sctx->pragma_once, realpath(tok->file->name, NULL), (void *)1);
      return skip_line(sctx, tok->next->next);
    }
    return pass_line(sctx, cur, start);
  }

  // `#`-only line is legal. It's called a null directive.
  if (tok->at_bol)
    return tok;

  error_tok(sctx, tok, "invalid preprocessor directive");
}

void define_macro_cli(SlimccCtx *sctx, char *str) {
  Token *tok = tokenize(sctx, new_file(sctx, "<command-line>", str), NULL, NULL);
  Macro *m = read_macro_name(sctx, &tok, tok);

  Token head = {0};
  Token *cur = &head;
  bool has_eq = false;
  for (; tok->kind != TK_EOF;) {
    if (!has_eq && consume(&tok, tok, "=")) {
      tok->has_space = true;
      has_eq = true;
      continue;
    }
    cur = cur->next = tok;
    tok = tok->next;
  }
  if (!has_eq) {
    cur = cur->next = make_token(sctx, "1", tok, NULL);
    cur->has_space = true;
  }
  cur->next = tok;
  m->body = head.next;
}

void define_macro(SlimccCtx *sctx, char *name, char *buf) {
  new_macro(sctx, name, true)->body = tokenize(sctx, new_file(sctx, "<built-in>", buf), NULL, NULL);
}

void undef_macro(SlimccCtx *sctx, char *name) {
  hashmap_delete(&sctx->macros, name);
}

static void add_builtin(SlimccCtx *sctx, char *name, macro_handler_fn *fn, bool align) {
  Macro *m = new_macro(sctx, name, true);
  m->handler = fn;
  m->align = align;
}

static Token *file_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = start;
  if (tok->origin)
    tok = tok->origin;
  return new_str_token(sctx, sctx->display_files.data[tok->file->display_file_no], start);
}

static Token *line_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = start;
  if (tok->origin)
    tok = tok->origin;
  int64_t val = tok->line_no;
  val += tok->display_line_no;
  val += tok->file->line_delta;
  return new_num_token(sctx, val, tok, start->next);
}

// __COUNTER__ is expanded to serial values starting from 0.
static Token *counter_macro(SlimccCtx *sctx, Token *start) {
  static uint32_t i;
  if (i > 2147483648)
    error_tok(sctx, start, "__COUNTER__ exceeded 2147483648");
  return new_num_token(sctx, i++, start, start->next);
}

// __DATE__ is expanded to the current date, e.g. "May 17 2020".
static Token *date_macro(SlimccCtx *sctx, Token *start) {
  static char *str;
  if (!str) {
    if (!sctx->cur_time)
      sctx->cur_time = localtime(&(time_t){time(NULL)});

    static char mon[][4] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    str = format("\"%s %2d %d\"", mon[sctx->cur_time->tm_mon], sctx->cur_time->tm_mday,
                 sctx->cur_time->tm_year + 1900);
  }
  return make_token(sctx, str, start, start->next);
}

// __TIME__ is expanded to the current time, e.g. "13:34:03".
static Token *time_macro(SlimccCtx *sctx, Token *start) {
  static char *str;
  if (!str) {
    if (!sctx->cur_time)
      sctx->cur_time = localtime(&(time_t){time(NULL)});

    str = format("\"%02d:%02d:%02d\"", sctx->cur_time->tm_hour, sctx->cur_time->tm_min,
                 sctx->cur_time->tm_sec);
  }
  return make_token(sctx, str, start, start->next);
}

// __TIMESTAMP__ is expanded to a string describing the last
// modification time of the current file. E.g.
// "Fri Jul 24 01:32:50 2020"
static Token *timestamp_macro(SlimccCtx *sctx, Token *start) {
  static char *str;
  static char buf[30];
  if (!str) {
    struct stat st;
    if (stat(start->file->name, &st) != 0) {
      str = "\"??? ??? ?? ??:??:?? ????\"";
    } else {
      #ifdef _WIN32
      if (ctime_s(&buf[1], sizeof(buf) - 1, &st.st_mtime) != 0) {
        str = "\"??? ??? ?? ??:??:?? ????\"";
      } else {
        buf[0] = buf[25] = '\"';
        buf[26] = '\0';
        str = buf;
      }
      #else
      ctime_r(&st.st_mtime, &buf[1]);
      buf[0] = buf[25] = '\"';
      buf[26] = '\0';
      str = buf;
      #endif
    }
  }
  return make_token(sctx, str, start, start->next);
}

static Token *base_file_macro(SlimccCtx *sctx, Token *start) {
  return new_str_token(sctx, sctx->base_file, start);
}

static Token *pragma_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = start->next;
  Token *str_tok = NULL;

  for (int progress = 0;;) {
    if (tok->kind == TK_EOF)
      error_tok(sctx, start, "unterminated _Pragma sequence");

    pop_macro_lock(sctx, tok);
    if (expand_macro(sctx, &tok, tok, false))
      continue;

    switch (progress++) {
    case 0: {
      tok = skip(sctx, tok, "(");
      continue;
    }
    case 1:
      if (tok->kind != TK_STR && tok->kind != TK_ASM_STR)
        error_tok(sctx, tok, "expected string literal");
      str_tok = tok;
      tok = tok->next;
      continue;
    case 2: {
      tok = skip(sctx, tok, ")");
      tok->at_bol = true;
    }
    }
    break;
  }
  char *buf = format("#pragma %s", str_tok->str);

  Token *end;
  Token *hash = tokenize_buf(sctx, buf, start, &end);
  hash->at_bol = true;
  end->next = tok;
  return hash;
}

static Token *has_include_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");

  char *dir = NULL;
  char *filename = read_include_filename(sctx, split_paren(sctx, &tok, tok), &dir);
  bool found = search_include_paths(sctx, filename, dir);

  pop_macro_lock_until(sctx, start, tok);
  return new_bool_int_token(sctx, found, start, tok);
}

static Token *has_include_next_macro(SlimccCtx *sctx, Token *start) {
  Token *file_tok = skip(sctx, start->next, "(");
  InclIdx idx = file_tok->file->incl_idx + 1;
  char *dir = NULL;
  Token *end;
  char *filename = read_include_filename(sctx, split_paren(sctx, &end, file_tok), &dir);
  bool found = search_include_paths2(sctx, filename, dir, &idx);

  pop_macro_lock_until(sctx, start, end);
  return new_bool_int_token(sctx, found, start, end);
}

static Token *has_embed_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");
  Token *end;

  char *dir = NULL;
  char *filename = read_filename(sctx, &tok, split_paren(sctx, &end, tok), &dir);
  char *path = search_include_paths(sctx, filename, dir);
  Token *tok2 = embed_file(sctx, NULL, tok, path, start->next->next);

  pop_macro_lock_until(sctx, start, end);
  tok2->next = end;
  return tok2;
}

static Token *has_attribute_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");

  bool val = is_supported_attr(sctx, tok);

  tok = skip(sctx, tok->next, ")");
  pop_macro_lock_until(sctx, start, tok);
  return new_bool_int_token(sctx, val, start, tok);
}

static Token *has_c_attribute_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");

  char *str = supported_c_attr(sctx, &tok, tok, NULL);

  tok = skip(sctx, tok->next, ")");
  pop_macro_lock_until(sctx, start, tok);
  return make_token(sctx, str ? str : "0", start, tok);
}

static Token *has_builtin_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");

  bool has_it = equal(tok, "__builtin_alloca") ||
                equal(tok, "__builtin_constant_p") ||
                equal(tok, "__builtin_expect") ||
                equal(tok, "__builtin_expect_with_probability") ||
                equal(tok, "__builtin_extract_return_addr") ||
                equal(tok, "__builtin_frame_address") ||
                equal(tok, "__builtin_offsetof") ||
                equal(tok, "__builtin_add_overflow") ||
                equal(tok, "__builtin_sub_overflow") ||
                equal(tok, "__builtin_mul_overflow") ||
                equal(tok, "__builtin_return_address") ||
                equal(tok, "__builtin_types_compatible_p") ||
                equal(tok, "__builtin_unreachable") ||
                equal(tok, "__builtin_c23_va_start") ||
                equal(tok, "__builtin_va_start") ||
                equal(tok, "__builtin_va_copy") ||
                equal(tok, "__builtin_va_end") ||
                equal(tok, "__builtin_va_arg");

  tok = skip(sctx, tok->next, ")");
  pop_macro_lock_until(sctx, start, tok);
  return new_bool_int_token(sctx, has_it, start, tok);
}

static Token *has_extension_macro(SlimccCtx *sctx, Token *start) {
  Token *tok = skip(sctx, start->next, "(");

  // Check clang/include/clang/Basic/Features.def, gcc/c/c-objc-common.cc
  bool has_it = equal(tok, "c_alignas") ||
                equal(tok, "c_alignof") ||
                equal(tok, "c_atomic") ||
                equal(tok, "c_generic_selections") ||
                equal(tok, "c_static_assert") ||
                equal(tok, "c_thread_local") ||
                equal(tok, "cxx_binary_literals") ||
                equal(tok, "c_fixed_enum") ||
                equal(tok, "c_countof") ||
                equal(tok, "gnu_asm") ||
                equal(tok, "gnu_asm_goto_with_outputs") ||
                equal(tok, "gnu_asm_goto_with_outputs_full");

  tok = skip(sctx, tok->next, ")");
  pop_macro_lock_until(sctx, start, tok);
  return new_bool_int_token(sctx, has_it, start, tok);
}

void init_macros(SlimccCtx *sctx) {
  arena_on(sctx, &sctx->pp_arena);

  define_macro(sctx, "__slimcc__", "1");
  sctx->macro_head = sctx->macro_defs;

  define_macro(sctx, "__STDC_EMBED_EMPTY__", "2");
  define_macro(sctx, "__STDC_EMBED_FOUND__", "1");
  define_macro(sctx, "__STDC_EMBED_NOT_FOUND__", "0");
  define_macro(sctx, "__STDC_HOSTED__", "1");
  define_macro(sctx, "__STDC_NO_COMPLEX__", "1");
  define_macro(sctx, "__STDC_UTF_16__", "1");
  define_macro(sctx, "__STDC_UTF_32__", "1");
  define_macro(sctx, "__STDC__", "1");

  define_macro(sctx, "__STDC_DEFER_TS25755__", "1");

  define_macro(sctx, "__C99_MACRO_WITH_VA_ARGS", "1");
  define_macro(sctx, "__USER_LABEL_PREFIX__", "");

  define_macro(sctx, "__unix", "1");
  define_macro(sctx, "__unix__", "1");

  define_macro(sctx, "__BYTE_ORDER__", "1234");
  define_macro(sctx, "__ORDER_BIG_ENDIAN__", "4321");
  define_macro(sctx, "__ORDER_LITTLE_ENDIAN__", "1234");

  define_macro(sctx, "__CHAR_BIT__", "8");
  define_macro(sctx, "__BITINT_MAXWIDTH__", "65535");

  define_macro(sctx, "__amd64", "1");
  define_macro(sctx, "__amd64__", "1");
  define_macro(sctx, "__x86_64", "1");
  define_macro(sctx, "__x86_64__", "1");

  add_builtin(sctx, "__DATE__", date_macro, true);
  add_builtin(sctx, "__TIME__", time_macro, true);
  add_builtin(sctx, "__FILE__", file_macro, true);
  add_builtin(sctx, "__LINE__", line_macro, true);
  add_builtin(sctx, "__COUNTER__", counter_macro, true);
  add_builtin(sctx, "__TIMESTAMP__", timestamp_macro, true);
  add_builtin(sctx, "__BASE_FILE__", base_file_macro, true);

  add_builtin(sctx, "_Pragma", pragma_macro, false);

  add_builtin(sctx, "__has_attribute", has_attribute_macro, true);
  add_builtin(sctx, "__has_c_attribute", has_c_attribute_macro, true);
  add_builtin(sctx, "__has_builtin", has_builtin_macro, true);
  add_builtin(sctx, "__has_extension", has_extension_macro, true);
  add_builtin(sctx, "__has_include", has_include_macro, true);
  add_builtin(sctx, "__has_include_next", has_include_next_macro, true);
  add_builtin(sctx, "__has_embed", has_embed_macro, true);
}

typedef enum {
  STR_NONE,
  STR_UTF8,
  STR_UTF16,
  STR_UTF32,
  STR_WIDE,
} StringKind;

static StringKind getStringKind(Token *tok) {
  if (!strcmp(tok->loc, "u8"))
    return STR_UTF8;

  switch (tok->loc[0]) {
  case '"': return STR_NONE;
  case 'u': return STR_UTF16;
  case 'U': return STR_UTF32;
  case 'L': return STR_WIDE;
  }
  internal_error();
}

// Concatenate adjacent string literals into a single string literal
// as per the C spec.
static void join_adjacent_string_literals(SlimccCtx *sctx, Token *tok) {
  Token *end = tok->next->next;
  while (end->kind == TK_STR)
    end = end->next;

  // If regular string literals are adjacent to wide string literals,
  // regular string literals are converted to the wide type.
  StringKind kind = getStringKind(tok);
  Type *basety = tok->ty->base;

  for (Token *t = tok->next; t != end; t = t->next) {
    StringKind k = getStringKind(t);
    if (kind == STR_NONE) {
      kind = k;
      basety = t->ty->base;
    } else if (k != STR_NONE && kind != k) {
      error_tok(sctx, t, "unsupported non-standard concatenation of string literals");
    }
  }

  if (basety->size > 1)
    for (Token *t = tok; t != end; t = t->next)
      if (t->ty->base->size == 1)
        tokenize_string_literal(sctx, t, basety);

  // Concatenate adjacent string literals.
  int len = tok->ty->array_len;
  for (Token *t = tok->next; t != end; t = t->next)
    len = len + t->ty->array_len - 1;

  char *buf = calloc(basety->size, len);

  int i = 0;
  for (Token *t = tok; t != end; t = t->next) {
    memcpy(buf + i, t->str, t->ty->size);
    i = i + t->ty->size - t->ty->base->size;
  }

  tok->ty = array_of(basety, len);
  tok->str = buf;
  tok->next = end;
}

static bool is_gnu_attr(Token *tok) {
#define PutAttr(str)                             \
  do {                                           \
    hashmap_put(&map, str, (void *)1);           \
    hashmap_put(&map, "__" str "__", (void *)1); \
  } while (0)

  static HashMap map;
  if (map.capacity == 0) {
    PutAttr("alias");
    PutAttr("aligned");
    PutAttr("cleanup");
    PutAttr("common");
    PutAttr("nocommon");
    PutAttr("constructor");
    PutAttr("destructor");
    PutAttr("gnu_inline");
    PutAttr("naked");
    PutAttr("noreturn");
    PutAttr("packed");
    PutAttr("returns_twice");
    PutAttr("section");
    PutAttr("used");
    PutAttr("weak");
  }
  return hashmap_get2(&map, tok->loc, tok->len);
}

static bool is_supported_attr(SlimccCtx *sctx, Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(sctx, tok, "expected attribute name");

  return is_gnu_attr(tok);
}

static char *supported_c_attr(SlimccCtx *sctx, Token **rest, Token *tok, Token **vendor_out) {
  Token *vendor = NULL;
  if (tok->kind == TK_IDENT && equal(tok->next, "::")) {
    vendor = tok;
    tok = tok->next->next;
  }
  if(vendor_out)
    *vendor_out = vendor;
  *rest = tok;

  if (tok->kind != TK_IDENT)
    error_tok(sctx, tok, "expected attribute name");

  if (vendor && equal_ext(vendor, "gnu") && is_gnu_attr(tok))
    return "1";

  if (equal_ext(tok, "noreturn") || equal_ext(tok, "_Noreturn"))
    return "202311L";

  return NULL;
}

static void filter_attr(SlimccCtx *sctx, Token *tok, Token **lst, bool is_bracket) {
  bool first = true;
  for (;; first = false) {
    bool has_comma = false;
    while (consume(&tok, tok, ","))
      has_comma = true;

    if (tok->kind == TK_EOF)
      break;
    if (!first && !has_comma)
      error_tok(sctx, tok, "expected ','");

    bool is_supported;
    Token *vendor = NULL;
    if (is_bracket)
      is_supported = supported_c_attr(sctx, &tok, tok, &vendor);
    else
      is_supported = is_supported_attr(sctx, tok);

    Token *start = tok;
    if (consume(&tok, tok->next, "("))
      tok = skip_paren(sctx, tok);
    else
      tok = tok->next;

    {
      Token head = {0};
      Token *cur = &head;
      for (Token *t = start; t != tok; t = t->next)
        cur = cur->next = copy_token(sctx, t);
      cur->next = new_eof(sctx, tok);

      *lst = (*lst)->attr_next = preprocess3(sctx, head.next);
      (*lst)->kind = is_bracket ? TK_BATTR : TK_ATTR;
      (*lst)->attr_supported = is_supported;
      (*lst)->attr_next = NULL;
      (*lst)->attr_vendor = vendor ? copy_token(sctx, vendor) : NULL;
    }
  }
}

static void stash_attr(SlimccCtx *sctx, Token *tok, Token *head, Token **attr_cur) {
  tok->attr_next = head->attr_next;
  head->attr_next = NULL;
  *attr_cur = head;
}

static Token *preprocess3(SlimccCtx *sctx, Token *tok) {
  Token head = {0};
  Token *cur = &head;

  Token attr_head = {0};
  Token *attr_cur = &attr_head;
  bool add_newline = false;

  while (tok->kind != TK_EOF) {
    Token *start = tok;
    if (equal(tok, "__attribute__") || equal(tok, "__attribute")) {
      tok = skip(sctx, tok->next, "(");
      tok = skip(sctx, tok, "(");
      Token *list = split_paren(sctx, &tok, tok);
      filter_attr(sctx, list, &attr_cur, false);

      Token *free_end = tok;
      tok = skip(sctx, tok, ")");
      to_freelist(start, free_end);
      continue;
    }

    if (equal(tok, "[") && consume(&tok, tok->next, "[")) {
      Token *list = split_bracket(sctx, &tok, tok);
      filter_attr(sctx, list, &attr_cur, true);

      Token *free_end = tok;
      tok = skip(sctx, tok, "]");
      to_freelist(start, free_end);
      continue;
    }

    if (is_pragma(&tok, tok)) {
      if (equal(tok, "pack")) {
        tok = get_line(&cur, start);
        for (Token *t = start; t != tok; t = t->next)
          t->alloc_next = NULL;
        stash_attr(sctx, start, &attr_head, &attr_cur);
        add_newline = true;
        continue;
      }
      if (equal(tok, "message"))
        notice_tok(sctx, tok, "#pragma message");

      while (!tok->next->at_bol)
        tok = tok->next;
      Token *free_end = tok;
      tok = tok->next;
      to_freelist(start, free_end);
      continue;
    }

    switch (tok->kind) {
    case TK_IDENT:
      if (tok->has_ucn)
        convert_ucn_ident(sctx, tok);
      else
        tok->kind = ident_keyword(sctx, tok);
      break;
    case TK_STR:
      if (tok->next->kind == TK_STR)
        join_adjacent_string_literals(sctx, tok);
      break;
    case TK_UNICODE: error_tok(sctx, tok, "unallowed unicode character");
    }

    stash_attr(sctx, tok, &attr_head, &attr_cur);
    cur = cur->next = tok;
    tok = tok->next;

    if (add_newline) {
      add_newline = false;
      cur->at_bol = true;
    }
    continue;
  }
  cur->next = tok;
  return head.next;
}

static void include_files_cli(SlimccCtx *sctx, StringArray *arr, Token **cur) {
  for (int i = 0; i < arr->len; i++) {
    char *path = search_include_paths(sctx, arr->data[i], ".");
    if (ignore_missing_dep(sctx, path, arr->data[i], NULL))
      continue;

    preprocess2(sctx, tokenize_file(sctx, path, NULL, NULL), cur);
  }
}

Token *preprocess(SlimccCtx *sctx, char *file, StringArray *incls, StringArray *imacros) {
  sctx->base_file = file;

  Token head = {0};
  Token *cur = &head;

  include_files_cli(sctx, imacros, &cur);
  cur = &head;

  include_files_cli(sctx, incls, &cur);

  preprocess2(sctx, tokenize_file(sctx, file, NULL, NULL), &cur);

  cur = cur->next;
  cur->is_root = true;

  return head.next;
}

Token *preprocess_data(SlimccCtx *sctx, char *filename, char *data, StringArray *incls, StringArray *imacros) {
  sctx->base_file = filename;
  
  Token head = {0};
  Token *cur = &head;
  
  include_files_cli(sctx, imacros, &cur);
  cur = &head;
  
  include_files_cli(sctx, incls, &cur);
  
  preprocess2(sctx, tokenize_file_data(sctx, filename, data, NULL, NULL), &cur);
  
  cur = cur->next;
  cur->is_root = true;
  
  return head.next;
}

Token *prepare_parse(SlimccCtx *sctx, Token *tok) {
  {
    Token *cur;
    Token *head = tokenize(sctx, new_file(sctx, "slimcc_builtins", "typedef struct {"
                                                       "  unsigned int gp_offset;"
                                                       "  unsigned int fp_offset;"
                                                       "  void *overflow_arg_area;"
                                                       "  void *reg_save_area;"
                                                       "} __builtin_va_list[1];"),
                           NULL, &cur);

    char *path = search_include_paths(sctx, "bitint_builtins", NULL);
    if (path) {
      Token *end;
      cur->next = tokenize_file(sctx, path, NULL, &end);
      cur = end;
    }
    for (Token *t = head; t; t = t->next)
      finalize_tok2(sctx, t, t);

    cur->next = tok;
    tok = head;
  }

  if (!(sctx->free_alloc = check_mem_usage())) {
    arena_off(sctx, &sctx->pp_arena);
    return preprocess3(sctx, tok);
  }

  Token *t = tok;
  for (t = sctx->last_alloc_tok; t;) {
    Token *tmp = t;
    t = t->alloc_next;

    if (!tmp->is_root)
      free(tmp);
  }

  tok = preprocess3(sctx, tok);

  for (t = sctx->tok_freelist; t;) {
    Token *tmp = t;
    t = t->next;
    free(tmp);
  }

  for(int i = 0 ; i < sctx->include_guards.capacity ; i++)
  {
    if(sctx->include_guards.buckets[i].val && sctx->include_guards.buckets[i].val != (void*)-1)
    {
      free(sctx->include_guards.buckets[i].val);
    }
  }
  
  free(sctx->macros.buckets);
  free(sctx->pragma_once.buckets);
  free(sctx->include_guards.buckets);
  free(sctx->cond_incl.data);
  arena_off(sctx, &sctx->pp_arena);
  return tok;
}
