#ifndef SLIMCC_H
#define SLIMCC_H

#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <glob.h>
#include <inttypes.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "slimcc_lib.h"

#if defined(__has_builtin)
# define SLIMCC_HAS_BUILTIN(x) __has_builtin(x)
#else
# define SLIMCC_HAS_BUILTIN(x) 0
#endif

#if defined(__has_attribute)
# define SLIMCC_HAS_ATTR(x) __has_attribute(x)
#else
# define SLIMCC_HAS_ATTR(x) 0
#endif

#if defined(__has_c_attribute)
# define SLIMCC_HAS_C_ATTR(x) __has_c_attribute(x)
#else
# define SLIMCC_HAS_C_ATTR(x) 0
#endif

#if defined(__has_feature)
# define SLIMCC_HAS_FEAT(x) __has_feature(x)
#else
# define SLIMCC_HAS_FEAT(x) 0
#endif

#if SLIMCC_HAS_FEAT(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
# define USE_ASAN
# include <sanitizer/asan_interface.h>
#endif

#if SLIMCC_HAS_FEAT(undefined_behavior_sanitizer) || defined(__SANITIZE_UNDEFINED__)
# define USE_UBSAN
#endif

#if SLIMCC_HAS_FEAT(type_sanitizer) || defined(__SANITIZE_TYPE__)
# define USE_TYSAN
#endif

#if defined(USE_ASAN) || defined(__FILC__)
# define EAGER_FREE 1
#else
# define EAGER_FREE 0
#endif

#if (defined(__GNUC__) && __GNUC__ >= 3) || SLIMCC_HAS_ATTR(format)
# define FMTCHK(x, y) __attribute__((format(printf, (x), (y))))
#else
# define FMTCHK(x, y)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define NORETURN _Noreturn
#elif __GNUC__ >= 3 || SLIMCC_HAS_ATTR(noreturn)
# define NORETURN __attribute__((noreturn))
#elif SLIMCC_HAS_C_ATTR(noreturn)
# define NORETURN [[noreturn]]
#else
# define NORETURN
#endif

#if defined(__SIZEOF_LONG_LONG__) && __SIZEOF_LONG_LONG__ == 8
# if SLIMCC_HAS_BUILTIN(__builtin_popcountll)
#  define Pop64(x) __builtin_popcountll((uint64_t)(x))
# endif
# if SLIMCC_HAS_BUILTIN(__builtin_ctzll)
#  define Ctz64(x) __builtin_ctzll((uint64_t)(x))
# endif
# if SLIMCC_HAS_BUILTIN(__builtin_clzll)
#  define Clz64(x) __builtin_clzll((uint64_t)(x))
# endif
#endif

#if defined(__has_include)
# if __has_include(<stdbit.h>)
#  include <stdbit.h>
#  ifndef Pop64
#   define Pop64(x) (int)stdc_count_ones((uint64_t)(x))
#  endif
#  ifndef Ctz64
#   define Ctz64(x) (int)stdc_trailing_zeros((uint64_t)(x))
#  endif
#  ifndef Clz64
#   define Clz64(x) (int)stdc_leading_zeros((uint64_t)(x))
#  endif
# endif
#endif

#ifndef Pop64
# define Pop64(x) _pop64_impl(x)
static inline int _pop64_impl(uint64_t v) {
  int cnt = 0;
  for (; v; cnt++)
    v &= v - 1;
  return cnt;
}
#endif

#ifndef Ctz64
# define Ctz64(x) _ctz64_impl(x)
static inline int _ctz64_impl(uint64_t v) {
  return Pop64((v & -v) - 1);
}
#endif

#ifdef __clang__
# pragma clang diagnostic ignored "-Wswitch"
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

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define S_ASSERT(x) static_assert((x), "");
#else
# define S_ASSERT(x)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define ANON_UNION_START union {
# define ANON_UNION_END \
   }                    \
   ;
#else
# define ANON_UNION_START
# define ANON_UNION_END
#endif

#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define Ucast(c) (unsigned int)(unsigned char)(c)
#define Inrange(c, x, y) ((Ucast(c) - Ucast(x)) <= (Ucast(y) - Ucast(x)))
#define Isdigit(c) Inrange((c), '0', '9')
#define Isalpha(c) Inrange((c) | 0x20, 'a', 'z')
#define Isalnum(c) (Isalpha(c) || Isdigit(c))
#define Isxdigit(c) (Isdigit(c) || Inrange((c) | 0x20, 'a', 'f'))
#define Casecmp(c, a) (((c) | 0x20) == a)

#ifdef BOOTSTRAP_NO_LDOUBLE
typedef double long_double_t;
#else
typedef long double long_double_t;
#endif

typedef Slimcc_Token       Token;
typedef Slimcc_Type        Type;
typedef Slimcc_File        File;
typedef Slimcc_EnumVal     EnumVal;
typedef Slimcc_Member      Member;
typedef Slimcc_Obj         Obj;
typedef Slimcc_HashEntry   HashEntry;
typedef Slimcc_HashMap     HashMap;
typedef Slimcc_TokenKind   TokenKind;
typedef Slimcc_Arena       Arena;
typedef Slimcc_QualMask    QualMask;
typedef Slimcc_TypeKind    TypeKind;
typedef Slimcc_StringArray StringArray;
typedef Slimcc_VarScope    VarScope;

typedef struct Node Node;
typedef struct Relocation Relocation;
typedef struct LocalLabel LocalLabel;
typedef union FPVal FPVal;
typedef struct AsmContext AsmContext;
typedef struct FuncObj FuncObj;
typedef struct SlashDelta SlashDelta;

typedef struct Slimcc_Ctx Slimcc_Ctx;

//
// alloc.c
//

char *arena_format(Arena *arena, const char *fmt, ...);
char *arena_strdup(Arena *arena, const char *str);
char *arena_copy_string(Arena *arena, const char *src, size_t len);
void arena_on(Arena *arena);
void arena_off(Arena *arena);
void *arena_calloc(Arena *a, size_t sz);
void *arena_malloc(Arena *a, size_t sz);

bool check_mem_usage(void);

extern Arena pp_arena;
extern bool free_alloc;

//
// hashmap.c
//

HashEntry *hashmap_get_or_insert(HashMap *map, const char *key, int keylen);
void *hashmap_get(HashMap *map, const char *key);
void *hashmap_get2(HashMap *map, const char *key, int keylen);
void hashmap_put(HashMap *map, const char *key, void *val);
void hashmap_put2(HashMap *map, const char *key, int keylen, void *val);
void hashmap_delete(HashMap *map, const char *key);
void hashmap_delete2(HashMap *map, const char *key, int keylen);
void hashmap_test(void);

// Represents a deleted hash entry
#define TOMBSTONE ((void *)-1)
#define TOMBSTONE_CASE ((uintptr_t)-1)

//
// strings.c
//

void strarray_push(StringArray *arr, const char *s);
char *format(const char *fmt, ...) FMTCHK(1, 2);

//
// tokenize.c
//

NORETURN void error_ice(const char *file, int32_t line);
NORETURN void error(const char *fmt, ...) FMTCHK(1, 2);
NORETURN void error_at(const char *loc, const char *fmt, ...) FMTCHK(2, 3);
NORETURN void error_tok(Token *tok, const char *fmt, ...) FMTCHK(2, 3);
void warn_tok(Token *tok, const char *fmt, ...) FMTCHK(2, 3);
void notice_tok(Token *tok, const char *fmt, ...) FMTCHK(2, 3);
void verror_at_tok(Token *tok, const char *fmt, va_list ap);
bool equal(Token *tok, const char *op);
bool equal_ext(Token *tok, const char *op);
Token *skip_tk(Token *tok, TokenKind);
bool consume(Token **rest, Token *tok, const char *str);
bool consume_tk(Token **rest, Token *tok, TokenKind kind);
Token *tokenize_file(const char *path, Token *tok, Token **end);
File *new_file(const char *name, const char *contents);
int add_display_file(const char *path);
void tokenize_string_literal(Token *tok, Type *basety);
Token *tokenize(File *file, SlashDelta *delta, Token **end);
void convert_pp_number(Token *tok, Node *node);
bool is_pp_token_int(Token *tok);
TokenKind ident_keyword(Token *tok);
void convert_ucn_ident(Token *tok);

#define internal_error() error_ice(__FILE__, __LINE__)

//
// preprocess.c
//

void init_macros(void);
void define_macro(const char *name, const char *buf);
void define_macro_cli(const char *str);
void undef_macro(const char *name);
void dump_defines(FILE *out);
Token *preprocess(const char *file, StringArray *incls, StringArray *macros);
Token *prepare_parse(Token *tok);
extern Token *last_alloc_tok;
extern Token *tok_freelist;

//
// parse.c
//

struct Relocation {
  Relocation *next;
  int offset;
  Node *label;
  Obj *var;
  int64_t addend;
};

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

// AST node
typedef enum {
  ND_NULL_STMT,
  ND_NULL_EXPR, // Do nothing
  ND_ADD,       // +
  ND_SUB,       // -
  ND_MUL,       // *
  ND_DIV,       // /
  ND_POS,       // unary +
  ND_NEG,       // unary -
  ND_MOD,       // %
  ND_BITAND,    // &
  ND_BITOR,     // |
  ND_BITXOR,    // ^
  ND_SHL,       // <<
  ND_SHR,       // >>
  ND_SAR,       // arithmetic >>
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LT,        // <
  ND_LE,        // <=
  ND_GT,        // >
  ND_GE,        // >=
  ND_ASSIGN,    // =
  ND_COND,      // ?:
  ND_COMMA,     // ,
  ND_MEMBER,    // . (struct member access)
  ND_ADDR,      // unary &
  ND_DEREF,     // unary *
  ND_NOT,       // !
  ND_BITNOT,    // ~
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_RETURN,    // "return"
  ND_IF,        // "if"
  ND_FOR,       // "for" or "while"
  ND_DO,        // "do"
  ND_SWITCH,    // "switch"
  ND_CASE,      // switch cases
  ND_DEFAULT,   // switch default case
  ND_BLOCK,     // { ... }
  ND_BREAK,
  ND_CONT,
  ND_GOTO,      // "goto"
  ND_GOTO_EXPR, // "goto" labels-as-values
  ND_LABEL,     // Labeled statement
  ND_LABEL_VAL, // [GNU] Labels-as-values
  ND_FUNCALL,   // Function call
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression
  ND_VAR,       // Variable
  ND_NUM,       // Integer
  ND_CAST,      // Type cast
  ND_INIT_SEQ,
  ND_ASM,      // "asm"
  ND_CAS,      // Atomic compare-and-swap
  ND_EXCH,     // Atomic exchange
  ND_VA_START, // "va_start"
  ND_VA_COPY,  // "va_copy"
  ND_VA_ARG,   // "va_arg"
  ND_CHAIN,
  ND_ALLOCA,
  ND_ALLOCA_ZINIT,
  ND_ARITH_ASSIGN,
  ND_POST_INCDEC,
  ND_CKD_ARITH,
  ND_FRAME_ADDR,
  ND_RTN_ADDR,
  ND_THREAD_FENCE,
  ND_UNREACHABLE,
  ND_UNKNOWN,
} NodeKind;

typedef union {
  uint64_t as64;
  uint32_t as32;
  uint8_t as8;
} BitBuf;

// AST node type
struct Node {
  Node *next;
  NodeKind kind;
  NodeKind arith_kind : 30; // Arithmetic Assignment
  bool no_label : 1;
  bool is_nonlval : 1;
  Type *ty;
  Token *tok; // Representative token

  DeferStmt *dfr_from;
  DeferStmt *dfr_dest;

  ANON_UNION_START
  // Misc
  struct {
    Node *lhs;
    Node *rhs;
    Node *target;
    Obj *var;
    Member *member;
  } m;

  // Numeric literal
  struct {
    int64_t val;
    BitBuf *bitint_data;
    long_double_t fval;
    enum {
      MATH_CONSTANT_NOT = 0,
      MATH_CONSTANT_NANF,
      MATH_CONSTANT_INFF,
      MATH_CONSTANT_NANSF,
      MATH_CONSTANT_NANS,
      MATH_CONSTANT_NANSL,
    } constant;
  } num;

  // Block or statement expression
  struct {
    Node *body;
    Node *local_labels;
    Node *result;
  } blk;

  // if, ?:, for, do, while, switch
  struct {
    Node *cond;
    Node *then;
    ANON_UNION_START
    Node *els;
    Node *for_init;
    Node *sw_default;
    ANON_UNION_END
    ANON_UNION_START
    Node *for_inc;
    ANON_UNION_END
    int64_t id;
  } ctrl;

  // labels
  struct {
    Node *next;
    int64_t id;
  } lbl;

  // case
  struct {
    Node *parent_sw;
    int64_t id;
  } cases;

  // break, continue, goto
  struct {
    Node *parent_loop;
    Node *target;
  } jmp;

  // Function call
  struct {
    Node *expr;
    Obj *rtn_buf;
    Obj *args;
  } call;

  // Atomic compare-and-swap
  struct {
    Node *addr;
    Node *old_val;
    Node *new_val;
  } cas;

  // GNU inline assembly
  struct {
    Token *str_tok;
    AsmParam *outputs;
    AsmParam *inputs;
    Token *clobbers;
    AsmParam *labels;
    AsmContext *ctx; // backend defined
  } gasm;
  ANON_UNION_END
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

Node *new_cast(Node *expr, Type *ty);
int64_t const_expr(Token **rest, Token *tok);
int64_t eval_sign_extend(Type *ty, int64_t val);
void eval_fp(Node *node, FPVal *fval);
Obj *parse(Token *tok);
Token *skip_paren(Token *tok);
Obj *new_lvar(Type *ty);
bool is_const_var(Obj *var);
bool is_const_expr(Node *node, int64_t *val);
bool is_const_fp(Node *node, FPVal *fval);
bool is_const_zero_bitint(Node *node);
Obj *eval_var_opt(Node *node, int *ofs, bool let_array, bool let_atomic);
bool equal_tok(Token *a, Token *b);
Obj *get_symbol_var(const char *);
Type *vla_cond_result_len(Type *ty1, Type *ty2, Type *base, Node **cond, Obj **cond_var);

//
// bitint.c
//

int32_t eval_bitint_first_set(int32_t bits, BitBuf *op);
bool eval_bitint_to_bool(int32_t bits, BitBuf *op);
void eval_bitint_sign_ext(int32_t bits, BitBuf *op, int32_t bits2, bool is_unsigned);
void eval_bitint_neg(int32_t bits, BitBuf *op);
void eval_bitint_bitnot(int32_t bits, BitBuf *op);
void eval_bitint_bitand(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_bitor(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_bitxor(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_shl(int32_t bits, BitBuf *src, BitBuf *dst, int32_t amount);
void eval_bitint_shr(int32_t bits, BitBuf *src, BitBuf *dst, int32_t amount,
                     bool is_unsigned);
void *eval_bitint_bitfield_load(int32_t bits, BitBuf *src, BitBuf *dst, int32_t width,
                                int32_t ofs, bool is_unsigned);
void eval_bitint_bitfield_save(int32_t bits, BitBuf *src, BitBuf *dst, int32_t width,
                               int32_t ofs);
void eval_bitint_add(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_sub(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_mul(int32_t bits, BitBuf *lh, BitBuf *rh);
void eval_bitint_div(int32_t bits, BitBuf *lh, BitBuf *rh, bool is_unsigned, bool is_div);
int eval_bitint_cmp(int32_t bits, BitBuf *lh, BitBuf *rh, bool is_unsigned);

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

extern Type *slimcc_ty_void;
extern Type *slimcc_ty_bool;
extern Type *slimcc_ty_nullptr;

extern Type *slimcc_ty_pchar;

extern Type *slimcc_ty_char;
extern Type *slimcc_ty_short;
extern Type *slimcc_ty_int;
extern Type *slimcc_ty_long;
extern Type *slimcc_ty_llong;

extern Type *slimcc_ty_uchar;
extern Type *slimcc_ty_ushort;
extern Type *slimcc_ty_uint;
extern Type *slimcc_ty_ulong;
extern Type *slimcc_ty_ullong;

extern Type *slimcc_ty_float;
extern Type *slimcc_ty_double;
extern Type *slimcc_ty_ldouble;

extern Type *slimcc_ty_size_t;
extern Type *slimcc_ty_ptrdiff_t;

extern Type *slimcc_ty_intmax_t;
extern Type *slimcc_ty_uintmax_t;
extern Type *slimcc_ty_eval_int;

extern Type *slimcc_ty_char16_t;
extern Type *slimcc_ty_char32_t;
extern Type *slimcc_ty_wchar_t;

extern Type *slimcc_enum_ty[8];
extern EnumType slimcc_ety_of_int;

bool is_pow_of_two(uint64_t val);
bool is_integer(Type *ty);
bool is_int_class(Type *ty);
bool is_flonum(Type *ty);
bool is_numeric(Type *ty);
bool is_array(Type *ty);
bool is_decay_ty(Type *ty);
bool is_bitfield(Node *node);
bool is_redundant_cast(Node *expr, Type *ty);
bool is_compatible(Type *t1, Type *t2);
bool is_compatible2(Type *t1, Type *t2);
bool is_record_compat(Type *t1, Type *t2, bool is_redecl);
bool is_null_ptr_constant(Node *node);
bool is_ptr(Type *ty);
int next_pow_of_two(int val);
int64_t bit_size(Type *ty);
int64_t bitint_buffer_size(Type *ty);
int32_t bitfield_footprint(Member *mem);
void init_ty_lp64(void);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *ptr_decay(Type *ty);
void ptr_convert(Node **node);
Type *func_type(Type *return_ty, Token *tok);
Type *get_func_ty(Node *node);
Type *array_of(Type *base, int64_t size);
Type *vla_of(Type *base, Node *expr, int64_t arr_len);
Type *new_type(TypeKind kind, int64_t size, int align);
Type *new_bitint(int64_t width, Token *tok);
Type *unqual(Type *ty);
Type *tyof_unqual(Type *ty);
Type *new_derived_type(Type *newty, QualMask qual, Type *ty);
bool chk_qual_type(QualMask qual, Type *ty);
Type *aligned_type(int align, Type *ty);
Type *add_qual(QualMask msk, Type *ty, Token *tok);
bool mem_iter(Member **mem);
Node *assign_cast(Type *to, Node *expr);

void _add_type(Node *node);
#define add_type(_n) \
  do {               \
    if (!(_n)->ty)   \
      _add_type(_n); \
  } while (0)

//
// codegen.c
//

void prepare_funcall(Node *node, Scope *scope);
void prepare_inline_asm(Node *node);
int64_t align_to(int64_t n, int64_t align);
bool va_arg_need_copy(Type *ty);
bool bitint_rtn_need_copy(size_t width);
void emit_text(Obj *fn);

//
// unicode.c
//

int encode_utf8(char *buf, uint32_t c);
uint32_t decode_utf8(const char **new_pos, const char *p);
bool is_ident1(uint32_t c);
bool is_ident2(uint32_t c);
int display_width(const char *p, int len);

//
// platform.c
//

void platform_init_cc1(void);
void platform_init_driver(void);
void platform_stdinc_paths(StringArray *paths);
void platform_search_dirs(StringArray *paths);
void run_assembler(StringArray *as_args, const char *input, const char *output);
void run_linker(StringArray *paths, StringArray *args, const char *output);

//
// main.c
//

typedef struct {
  const char *arg;
  bool is_def;
} MacroChange;

typedef struct {
  MacroChange *data;
  int capacity;
  int len;
} MacroChangeArr;

typedef struct JumpContext JumpContext;

typedef struct FuncContext FuncContext;
struct FuncContext {
  Obj *fn;
  Obj *fnname;
  Node *gotos;
  Node *labels;
  DeferStmt *defr;
  bool use_vla;
  bool dont_dealloc_vla;
  bool is_static_init_context;
  Token *defr_ctx;
};

typedef struct {
  Token *tok;
  bool is_else;
  bool been_active;
} CondIncl;

typedef Token *macro_handler_fn(Token *);
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

struct Slimcc_Ctx {
  // alloc
  Slimcc_Arena cc1_arena;
  Slimcc_Arena ast_arena;
  Slimcc_Arena pp_arena;
  bool free_alloc;
  Slimcc_Pool *pool_freelist;

  // main
  StringArray opt_imacros;
  StringArray opt_include;

  StringArray sysincl_paths;
  StringArray dep_files;
  StringArray tmpfiles;
  const char *tmp_folder;
  StringArray as_args;
  MacroChangeArr macrodefs;
  int incl_cnt;

  // parse
  struct JumpContext {
    JumpContext *next;
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
  
  Scope *scope;
  bool *eval_recover;

  // codegen
  // those 3 will probably need to be deleted
  Obj *globals;
  HashMap symbols;
  FuncContext *fnctx;

  // pp
  struct {
    CondIncl *data;
    int capacity;
    int cnt;
  } cond_incl;

  Macro *locked_macros;
  HashMap macros;
  HashMap pragma_once;
  HashMap include_guards;
  Token *last_alloc_tok;
  Token *tok_freelist;
  
  const char *base_file;
  struct tm *cur_time;

  // tokenize
  File *current_file;
  // True if the current position is at the beginning of a line
  bool at_bol;
  // True if the current position follows a space character
  bool has_space;

  // type
  Type *vp;
};

typedef enum {
  LT_RELO,
  LT_SHARED,
  LT_DYNAMIC,
  LT_STATIC_PIE,
  LT_STATIC,
  LT_PIE,
} LinkType;

NORETURN void cleanup_exit(int status);
bool file_exists(const char *path);
bool in_sysincl_path(int idx);
bool ignore_missing_dep(const char *path, const char *filename, Token *tok);
void add_dep_file(const char *path, bool is_sys);
void add_include_path(StringArray *arr, const char *s);

#endif
