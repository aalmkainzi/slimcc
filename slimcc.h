#ifndef SLIMCC_H
#define SLIMCC_H

#define _CRT_DECLARE_NONSTDC_NAMES 1

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "slimcc_lib.h"

#if defined(__SANITIZE_ADDRESS__)
# define USE_ASAN 1
#elif defined(__has_feature)
# if __has_feature(address_sanitizer)
#  define USE_ASAN 1
# endif
#endif

#ifdef __clang__
# pragma clang diagnostic ignored "-Wswitch"
#endif

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define Ucast(c) (unsigned int)(unsigned char)(c)
#define Inrange(c, x, y) ((Ucast(c) - Ucast(x)) <= (Ucast(y) - Ucast(x)))
#define Isdigit(c) Inrange(c, '0', '9')
#define Isalnum(c) (Inrange((c) | 0x20, 'a', 'z') || Isdigit(c))
#define Isxdigit(c) (Isdigit(c) || Inrange((c) | 0x20, 'a', 'f'))
#define Casecmp(c, a) (((c) | 0x20) == a)

#if defined(__GNUC__) && (__GNUC__ >= 3)
# define FMTCHK(x, y) __attribute__((format(printf, (x), (y))))
# define NORETURN __attribute__((noreturn))
#elif defined(_WIN32)
#define NORETURN __declspec(noreturn)
#elif defined(__has_attribute)
# if __has_attribute(format)
#  define FMTCHK(x, y) __attribute__((format(printf, (x), (y))))
# endif
# if __has_attribute(noreturn)
#  define NORETURN __attribute__((noreturn))
# endif
#endif

#ifndef FMTCHK
# define FMTCHK(x, y)
#endif
#ifndef NORETURN
# define NORETURN
#endif

#if defined(__has_builtin) && !(USE_ASAN || defined(__FILC__))
# if __has_builtin(__builtin_popcount)
#  define BITCNT_POP(x) __builtin_popcount(x)
# endif
#endif

#if defined(__GNUC__)
# define BUFF_CAST(_t, _ptr)  \
   __extension__({            \
    union U {                 \
      char _m1;               \
      _t _m2;                 \
    };                        \
    ((union U *)(_ptr))->_m2; \
   })
#else
# define BUFF_CAST(_t, _ptr) (*((_t *)(_ptr)))
#endif

#define Type Slimcc_Type
#define Node Slimcc_Node
#define Token Slimcc_Token
#define Member Slimcc_Member
#define Relocation Slimcc_Relocation
#define EnumVal Slimcc_EnumVal
#define File Slimcc_File
#define TokenKind Slimcc_TokenKind
#define Obj Slimcc_Obj
#define QualMask Slimcc_QualMask
#define TypeKind Slimcc_TypeKind
#define NodeKind Slimcc_NodeKind
#define Scope Slimcc_Scope
#define DeferStmt Slimcc_DeferStmt
#define VarScope Slimcc_VarScope
#define AsmParam Slimcc_AsmParam
#define CaseRange Slimcc_CaseRange
#define InclIdx Slimcc_InclIdx
#define FuncObj Slimcc_FuncObj

typedef struct LocalLabel LocalLabel;
typedef union FPVal FPVal;
typedef struct AsmContext AsmContext;
typedef struct FuncObj FuncObj;
typedef struct SlashDelta SlashDelta;

typedef struct SlimccCtx SlimccCtx;

//
// alloc.c
//

typedef struct Pool Pool;
typedef struct {
  Pool *cur;
  Pool *head;
  int used;
} Arena;

void arena_on(SlimccCtx *sctx, Arena *arena);
void arena_off(SlimccCtx *sctx, Arena *arena);
void *arena_calloc(SlimccCtx *sctx, Arena *a, size_t sz);
void *arena_malloc(SlimccCtx *sctx, Arena *a, size_t sz);
void *ast_arena_malloc(SlimccCtx *sctx, size_t sz);
void *ast_arena_calloc(SlimccCtx *sctx, size_t sz);

bool check_mem_usage(void);

//
// hashmap.c
//

typedef struct {
  char *key;
  int keylen;
  void *val;
} HashEntry;

typedef struct {
  HashEntry *buckets;
  int capacity;
  int used;
} HashMap;

HashEntry *hashmap_get_or_insert(HashMap *map, char *key, int keylen);
void *hashmap_get(HashMap *map, char *key);
void *hashmap_get2(HashMap *map, char *key, int keylen);
void hashmap_put(HashMap *map, char *key, void *val);
void hashmap_put2(HashMap *map, char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, char *key);
void hashmap_delete2(HashMap *map, char *key, int keylen);
void hashmap_test(void);

//
// strings.c
//

typedef struct {
  char **data;
  int capacity;
  int len;
} StringArray;

void strarray_push(StringArray *arr, const char *s);
char *format(char *fmt, ...) FMTCHK(1, 2);

//
// tokenize.c
//

NORETURN void error(char *fmt, ...) FMTCHK(1, 2) ;
NORETURN void error_ice(char *file, int32_t line) ;
NORETURN void error_at(SlimccCtx *sctx, char *loc, char *fmt, ...) FMTCHK(3, 4) ;
NORETURN void error_tok(SlimccCtx *sctx, Token *tok, char *fmt, ...) FMTCHK(3, 4) ;
void warn_tok(SlimccCtx *sctx, Token *tok, char *fmt, ...) FMTCHK(3, 4);
void notice_tok(SlimccCtx *sctx, Token *tok, char *fmt, ...) FMTCHK(3, 4);
void verror_at_tok(SlimccCtx *sctx, Token *tok, char *fmt, va_list ap);
bool equal(Token *tok, char *op);
bool equal_ext(Token *tok, char *op);
Token *skip(SlimccCtx *sctx, Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize_file(SlimccCtx *sctx, char *path, Token *tok, Token **end);
Token *tokenize_file_data(SlimccCtx *tctx, char *path, char *buf, Token *tok, Token **end);
File *new_file(SlimccCtx *sctx, char *name, char *contents);
int add_display_file(SlimccCtx *sctx, char *path);
void tokenize_string_literal(SlimccCtx *sctx, Token *tok, Type *basety);
Token *tokenize(SlimccCtx *sctx, File *file, SlashDelta *delta, Token **end);
void convert_pp_number(SlimccCtx *sctx, Token *tok, Node *node);
TokenKind ident_keyword(SlimccCtx *sctx, Token *tok);
void convert_ucn_ident(SlimccCtx *sctx, Token *tok);

#define internal_error() error_ice(__FILE__, __LINE__)

//
// preprocess.c
//

typedef struct MacroDef MacroDef;
struct MacroDef {
  MacroDef *next;
  char *name;
};

typedef struct {
  Token *tok;
  bool is_else;
  bool been_active;
} CondIncl;

void init_macros(SlimccCtx*);
void define_macro(SlimccCtx*, char *name, char *buf);
void define_macro_cli(SlimccCtx*, char *str);
void undef_macro(SlimccCtx*,char *name);
void dump_defines(FILE *out);
Token *preprocess(SlimccCtx*, char *file, StringArray *incls, StringArray *macros);
Token *preprocess_data(SlimccCtx *sctx, char *filename, char *data, StringArray *incls, StringArray *imacros);
Token *prepare_parse(SlimccCtx*, Token *tok);
Token *skip_line(SlimccCtx*, Token *tok);
bool is_pragma(Token **rest, Token *tok);

//
// parse.c
//

typedef enum {
  DF_VLA_DEALLOC,
  DF_CLEANUP_FN,
  DF_DEFER_STMT,
} DeferKind;

typedef struct DeferStmt DeferStmt;
struct DeferStmt {
  DeferKind kind;
  DeferStmt *next;
  Obj *vla;
  Node *cleanup_fn;
  Node *stmt;
};

typedef enum {
  ASMOP_NULL = 0,
  ASMOP_NUM,
  ASMOP_SYMBOLIC,
  ASMOP_MEM,
  ASMOP_REG,
  ASMOP_FLAG,
} AsmOpKind;

typedef struct AsmParam AsmParam;
struct AsmParam {
  AsmParam *next;
  AsmOpKind kind;
  Token *name;
  Token *constraint;
  char *flag;
  AsmParam *match;
  Node *arg;
  Obj *ptr;
  Obj *var;
  int64_t val;
  uint64_t reg_constraint;
  int label_id;
  int reg;
  int var_asm_reg;
  bool is_mem_inreg;
  bool is_early_clobber;
  bool is_clobbered_x87;
};

typedef struct CaseRange CaseRange;
struct CaseRange {
  CaseRange *next;
  Node *label;
  int64_t lo;
  int64_t hi;
};

// Represents a block scope.
typedef struct Scope Scope;
struct Scope {
  Scope *parent;
  Scope *children;
  Scope *sibling_next;

  Obj *locals;
  LocalLabel *labels;
  Node *gotos;
  bool is_temporary;
  bool is_stmt;
  bool is_fn_base;
  bool has_label;

  HashMap vars;
  HashMap tags;
};


Node *new_cast(SlimccCtx *sctx, Node *expr, Type *ty);
int64_t const_expr(SlimccCtx *sctx, Token **rest, Token *tok);
int64_t eval_sign_extend(Type *ty, int64_t val);
void eval_fp(SlimccCtx*, Node *node, FPVal *fval);
Obj *parse(SlimccCtx *sctx, Token *tok);
Token *skip_paren(SlimccCtx *sctx, Token *tok);
Obj *new_lvar(SlimccCtx*, Type *ty);
bool is_const_var(Obj *var);
bool is_const_expr(SlimccCtx*, Node *node, int64_t *val);
bool is_const_fp(SlimccCtx*, Node *node, FPVal *fval);
bool is_const_zero_bitint(SlimccCtx*,Node *node);
Obj *eval_var_opt(SlimccCtx*, Node *node, int *ofs, bool let_array, bool let_atomic);
bool equal_tok(Token *a, Token *b);
char *new_unique_name(void);
Obj *get_symbol_var(SlimccCtx*, char *);
Type *vla_cond_result_len(SlimccCtx*, Type *ty1, Type *ty2, Type *base, Node **cond, Obj **cond_var);

//
// bitint.c
//

int32_t eval_bitint_first_set(int32_t bits, void *lp);
bool eval_bitint_to_bool(int32_t bits, void *lp);
void eval_bitint_sign_ext(int32_t bits, void *lp, int32_t bits2, bool is_unsigned);
void eval_bitint_neg(int32_t bits, void *lp);
void eval_bitint_bitnot(int32_t bits, void *lp);
void eval_bitint_bitand(int32_t bits, void *lp, void *rp);
void eval_bitint_bitor(int32_t bits, void *lp, void *rp);
void eval_bitint_bitxor(int32_t bits, void *lp, void *rp);
void eval_bitint_shl(int32_t bits, void *sp, void *dp, int32_t amount);
void eval_bitint_shr(int32_t bits, void *sp, void *dp, int32_t amount, bool is_unsigned);
void *eval_bitint_bitfield_load(int32_t bits, void *sp, void *dp, int32_t width,
                                int32_t ofs, bool is_unsigned);
void eval_bitint_bitfield_save(int32_t bits, void *sp, void *dp, int32_t width,
                               int32_t ofs);
void eval_bitint_add(int32_t bits, void *lp, void *rp);
void eval_bitint_sub(int32_t bits, void *lp, void *rp);
void eval_bitint_mul(int32_t bits, void *lp, void *rp);
void eval_bitint_div(int32_t bits, void *lp, void *rp, bool is_unsigned, bool is_div);
int eval_bitint_cmp(int32_t bits, void *lp, void *rp, bool is_unsigned);

//
// type.c
//

typedef enum {
  ETY_I8 = 0,
  ETY_U8,
  ETY_I16,
  ETY_U16,
  ETY_I32,
  ETY_U32,
  ETY_I64,
  ETY_U64,
} EnumType;

union FPVal {
  uint64_t chunk[2];
  uint32_t chunk32[4];
  long_double_t ld;
  double d;
  float f;
};

bool is_pow_of_two(uint64_t val);
bool is_integer(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_array(Type *ty);
bool is_decay_ty(Type *ty);
bool is_bitfield(Node *node);
bool is_redundant_cast(Node *expr, Type *ty);
bool is_compatible(SlimccCtx *,Type *t1, Type *t2);
bool is_compatible2(SlimccCtx *sctx, Type *t1, Type *t2);
bool is_record_compat(SlimccCtx *sctx, Type *t1, Type *t2);
bool is_null_ptr_constant(SlimccCtx*, Node *node);
bool is_ptr(Type *ty);
int next_pow_of_two(int val);
int32_t bitfield_footprint(Member *mem);
void init_ty_lp64(SlimccCtx*);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *ptr_decay(SlimccCtx*, Type *ty);
void ptr_convert(SlimccCtx*, Node **node);
Type *func_type(SlimccCtx *sctx, Type *return_ty, Token *tok);
Type *get_func_ty(SlimccCtx *sctx, Node *node);
Type *array_of(Type *base, int64_t size);
Type *vla_of(SlimccCtx *sctx, Type *base, Node *expr, int64_t arr_len);
Type *new_type(TypeKind kind, int64_t size, int align);
Type *new_bitint(SlimccCtx *sctx, int64_t width, Token *tok);
void add_type_chk_const(SlimccCtx *sctx, Node *node);
void add_type(SlimccCtx *sctx, Node *node);
Type *unqual(Type *ty);
Type *new_derived_type(SlimccCtx*, Type *newty, QualMask qual, Type *ty, Token *tok);
Type *qual_type(SlimccCtx*, QualMask msk, Type *ty, Token *tok);
void cvqual_type(SlimccCtx*, Type **ty_p, Type *ty2);
bool mem_iter(Member **mem);
Node *assign_cast(SlimccCtx *sctx, Type *to, Node *expr);

//
// codegen.c
//

void prepare_funcall(Node *node, Scope *scope);
int64_t align_to(int64_t n, int64_t align);
// bool va_arg_need_copy(Type *ty);
// bool bitint_rtn_need_copy(size_t width);
// void emit_text(Obj *fn);

//
// unicode.c
//

int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(SlimccCtx *sctx, char **new_pos, char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
int display_width(SlimccCtx *sctx, char *p, int len);

//
// platform.c
//

void platform_init(SlimccCtx *sctx);
void platform_stdinc_paths(StringArray *paths);
void platform_search_dirs(StringArray *paths);
void run_assembler(StringArray *as_args, char *input, char *output);
void run_linker(StringArray *paths, StringArray *inputs, char *output);

//
// main.c
//

typedef enum { STD_C89, STD_C99, STD_C11, STD_C17, STD_C23 } StdVer;

typedef struct {
  char *arg;
  bool is_def;
} MacroChange;

typedef struct {
  MacroChange *data;
  int capacity;
  int len;
} MacroChangeArr;

bool file_exists(char *path);
bool in_sysincl_path(SlimccCtx *sctx, int idx);
bool ignore_missing_dep(SlimccCtx *sctx, char *path, char *filename, Token *tok);
void add_dep_file(char *path, bool is_sys);
char *find_dir_w_file(char *pattern);
void run_subprocess(char **argv);
void set_fpic(char *lvl);
void set_fpie(char *lvl);
void add_include_path(StringArray *arr, char *s);
void run_assembler_gnustyle(StringArray *as_args, char *input, char *output);
void run_linker_gnustyle(StringArray *paths, StringArray *inputs, char *output,
                         char *ldso_path, char *libpath, char *gcclibpath);

static char *string_dup(const char *s, size_t n)
{
  size_t len = strnlen(s, n);
  
  char *ret = malloc(len + 1);
  if (!ret) return NULL;
  
  memcpy(ret, s, len);
  ret[len] = '\0';
  
  return ret;
}

extern Type *ty_void;
extern Type *ty_bool;
extern Type *ty_nullptr;

extern Type *ty_pchar;

extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;
extern Type *ty_llong;

extern Type *ty_uchar;
extern Type *ty_ushort;
extern Type *ty_uint;
extern Type *ty_ulong;
extern Type *ty_ullong;

extern Type *ty_float;
extern Type *ty_double;
extern Type *ty_ldouble;

extern Type *ty_size_t;
extern Type *ty_ptrdiff_t;

extern Type *ty_char16_t;
extern Type *ty_char32_t;
extern Type *ty_wchar_t;

extern Type *enum_ty[8];
extern EnumType ety_of_int;

typedef struct SlimccCtx
{
  // sctx
  Arena ast_arena;
  Arena node_arena;
  Arena pp_arena;
  bool free_alloc;
  Token *last_alloc_tok;
  Token *tok_freelist;
  Pool *pool_freelist;
  
  // opts
  StringArray include_paths;
  StringArray iquote_paths;
  StringArray display_files;
  bool opt_werror;
  char *opt_visibility;
  StdVer opt_std;
  bool is_iso_std;
  bool opt_fdefer_ts;
  bool opt_short_enums;
  bool opt_gnu_keywords;
  bool opt_ms_anon_struct;
  
  StringArray opt_imacros;
  StringArray opt_include;
  
  StringArray input_args;
  StringArray sysincl_paths;
  MacroChangeArr macrodefs;
  int incl_cnt;
  
  // pp ctx
  struct Macro *locked_macros;
  MacroDef *macro_head;
  MacroDef *macro_defs;
  HashMap macros;
  HashMap pragma_once;
  HashMap include_guards;
  
  char *base_file;
  struct tm *cur_time;
  
  struct {
    CondIncl *data;
    int capacity;
    int cnt;
  } cond_incl;
  
  // parse ctx
  Obj *globals;
  Scope *scope;
  HashMap symbols;
  struct FuncContext *fnctx;
  bool *eval_recover;
  
  struct JumpContext {
    struct JumpContext *next;
    DeferStmt *dfr_lvl;
    Token *dfr_ctx;
    Token *labels;
    Node *node;
  } *jump_ctx;
  
  struct {
    int *data;
    int capacity;
    int cnt;
  } pack_stk;
  
  // tokenize ctx
  File *current_file;
  
  // True if the current position is at the beginning of a line
  bool at_bol;
  
  // True if the current position follows a space character
  bool has_space;
  
} SlimccCtx;

#endif
