#ifndef SLIMCC_LIB_H
#define SLIMCC_LIB_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Slimcc_Arena   Slimcc_Arena;
typedef struct Slimcc_Token   Slimcc_Token;
typedef struct Slimcc_Type    Slimcc_Type;
typedef struct Slimcc_File    Slimcc_File;
typedef struct Slimcc_EnumVal Slimcc_EnumVal;
typedef struct Slimcc_Member  Slimcc_Member;
typedef struct Slimcc_Obj     Slimcc_Obj;

typedef enum { STD_C89, STD_C94, STD_C99, STD_C11, STD_C17, STD_C23 } Slimcc_StdVer;

typedef enum {
  TK_IDENT,   // Identifiers
  TK_KEYWORD, // Keywords
  TK_STR,     // String literals
  TK_ASM_STR,
  TK_CHAR_LIT, // Character literals
  TK_CHAR_LIT_PPEV = TK_CHAR_LIT + 1,
  TK_PP_NUM, // Preprocessing numbers
  TK_PP_NUM_PPEV = TK_PP_NUM + 1,
  TK_INTMAX_NUM,
  TK_FMARK,  // Filemarkers for -E
  TK_PMARK,  // Placermarkers
  TK_ATTR,   // GNU attribute
  TK_BATTR,  // C23 attribute
  TK_PRAGMA, // #pragma's
  TK_EOF,    // End-of-file markers
  TK_INVALID,

  // Punctuators
  TK_PUNCT,
  TK_LPAREN,
  TK_RPAREN,
  TK_COMMA,
  TK_SEMI,
  TK_QMARK,
  TK_LBRACK,
  TK_RBRACK,
  TK_LCURLY,
  TK_RCURLY,
  TK_BITNOT,
  TK_LANGLE,
  TK_LANGLE_EQ,
  TK_LANGLE2,
  TK_RANGLE,
  TK_RANGLE_EQ,
  TK_RANGLE2,
  TK_NOT,
  TK_NOT_EQ,
  TK_REM,
  TK_MUL,
  TK_DIV,
  TK_XOR,
  TK_EQ2,
  TK_HASH,
  TK_HASH2,
  TK_DOT,
  TK_DOT3,
  TK_COLON,
  TK_COLON2,
  TK_AND,
  TK_AND2,
  TK_ADD,
  TK_ADD2,
  TK_SUB,
  TK_SUB2,
  TK_OR,
  TK_OR2,
  TK_ARROW,

  TK_EQ,
  TK_ADD_EQ,
  TK_SUB_EQ,
  TK_MUL_EQ,
  TK_DIV_EQ,
  TK_REM_EQ,
  TK_AND_EQ,
  TK_OR_EQ,
  TK_XOR_EQ,
  TK_LANGLE2_EQ,
  TK_RANGLE2_EQ,

  TK_PUNCT_END,

  TK_return,
  TK_if,
  TK_else,
  TK_for,
  TK_while,
  TK_do,
  TK_goto,
  TK_break,
  TK_continue,
  TK_switch,
  TK_case,
  TK_default,
  TK_sizeof,
  TK_Generic,
  TK_Countof,
  TK_alignof,
  TK_asm,
  TK_static_assert,
  TK_true,
  TK_false,
  TK_nullptr,
  TK_defer,
  TK_FUNCTION,
  TK_GNU_label,

  TK_TYPEKW,
  TK_void,
  TK_char,
  TK_short,
  TK_int,
  TK_long,
  TK_float,
  TK_double,
  TK_unsigned,
  TK_struct,
  TK_union,
  TK_enum,
  TK_typedef,
  TK_static,
  TK_extern,
  TK_auto,
  TK_register,
  TK_Atomic,
  TK_Noreturn,
  TK_BitInt,
  TK_auto_type,
  TK_alignas,
  TK_bool,
  TK_const,
  TK_constexpr,
  TK_inline,
  TK_restrict,
  TK_signed,
  TK_typeof,
  TK_typeof_unqual,
  TK_thread_local,
  TK_volatile,
  TK_TYPEKW_END,
} Slimcc_TokenKind;

typedef enum {
  TY_VOID,
  TY_BOOL,
  TY_PCHAR,
  TY_CHAR,
  TY_SHORT,
  TY_INT,
  TY_LONG,
  TY_LONGLONG,
  TY_FLOAT,
  TY_DOUBLE,
  TY_LDOUBLE,
  TY_ENUM,
  TY_PTR,
  TY_NULLPTR,
  TY_FUNC,
  TY_ARRAY,
  TY_VLA, // variable-length array
  TY_STRUCT,
  TY_UNION,
  TY_BITINT,
  TY_AUTO,
  TY_ASM,
} Slimcc_TypeKind;

typedef enum {
  Q_NONE = 0,
  Q_CONST = 1,
  Q_VOLATILE = 1 << 1,
  Q_ATOMIC = 1 << 2,
  Q_RESTRICT = 1 << 3,
} Slimcc_QualMask;

typedef enum {
  INCL_ABS = -2,
  INCL_REL = -1,
} Slimcc_InclIdx;

typedef struct {
  const char *key;
  int keylen;
  void *val;
} Slimcc_HashEntry;

typedef struct {
  Slimcc_HashEntry *buckets;
  int32_t capacity;
  int32_t used;
} Slimcc_HashMap;

typedef struct Slimcc_Pool Slimcc_Pool;
typedef struct Slimcc_Arena {
  Slimcc_Pool *cur;
  Slimcc_Pool *head;
  int used;
} Slimcc_Arena;

typedef struct {
  Slimcc_Obj *var;
  Slimcc_Type *type_def;
  Slimcc_EnumVal *enum_val;
} Slimcc_VarScope;

struct Slimcc_File {
  const char *name;
  const char *contents;
  int file_no;

  int display_file_no;
  int line_delta;
  Slimcc_InclIdx incl_idx;
  bool is_syshdr;
  bool is_placeholder;
};

struct Slimcc_Token {
  Slimcc_Token *next;
  Slimcc_TokenKind kind : 16;
  bool at_bol : 1;      // True if this token is at beginning of line
  bool has_space : 1;   // True if this token follows a space character
  bool dont_expand : 1; // True if a macro name is encountered during its expansion
  bool is_incl_guard : 1;
  bool is_root : 1;
  bool is_live : 1;
  bool has_ucn : 1;
  int len;         // Token length
  const char *loc; // Token location
  Slimcc_File *file;
  Slimcc_Token *origin; // If this is expanded from a macro, the original token
  int line_no;   // Line number
  int display_line_no;
  int display_file_no;
  Slimcc_Type *ty; // Used if TK_INT_NUM or TK_STR
  union {
    Slimcc_Token *attr_next;
    Slimcc_Token *alloc_next;
  };
  union {
    int64_t ival; // If kind is TK_INT_NUM, its value
    char *str;    // String literal contents including terminating '\0'
    Slimcc_Token *label_next;
  };
};

struct Slimcc_EnumVal {
  Slimcc_EnumVal *next;
  Slimcc_Token *name;
  int64_t val;
  Slimcc_Type *ty;
};

// Struct member
struct Slimcc_Member {
  Slimcc_Member *next;
  Slimcc_Type *ty;
  Slimcc_Token *name;
  int64_t offset;
  int idx;
  int alt_align;
  bool is_packed;
  
  bool is_bitfield;
  bool is_aligned_bitfield;
  int bit_offset;
  int bit_width;
};

struct Slimcc_Obj {
  Slimcc_Obj *next;
  char *name;
  Slimcc_Type *ty;
  bool is_local;
  bool is_live;
  bool is_used;
  bool is_compound_lit;
  bool is_string_lit;
  int alt_align;

  // Global variable or function
  bool is_definition;
  bool is_static;
  bool is_weak;
  bool is_static_lvar;
  Slimcc_Obj *static_lvars;
  char *alias_name;
  char *visibility;
  char *asm_name;

  // Global variable
  bool is_tls;
  bool is_common;
  bool is_nocommon;
  char *section_name;
  char *init_data;

  // constexpr variable
  char *constexpr_data;

  // Function
  bool export_fn;
  bool export_fn_gnu;
  bool is_gnu_inline;
  bool is_always_inline;
  bool is_naked;
  bool is_noreturn;
  bool returns_twice;
  bool dont_reuse_stk;
  bool dealloc_vla;
  bool is_ctor;
  bool is_dtor;
  uint16_t ctor_prior;
  uint16_t dtor_prior;
};

struct Slimcc_Type {
  Slimcc_TypeKind kind;
  int64_t size;
  int32_t align;
  bool is_unsigned;
  bool is_enum;
  bool is_int_enum;
  bool is_fixed_enum;
  Slimcc_QualMask qual;
  Slimcc_Type *origin;
  Slimcc_Type *decl_next; // forward declarations
  Slimcc_Token *tag;
  Slimcc_EnumVal *enums;

  // Pointer-to or array-of type.
  Slimcc_Type *base;

  // _BitInt
  int64_t bit_cnt;

  // Array
  int64_t array_len;

  // Variable-length array
  Slimcc_Obj *vla_len_val;

  // Struct
  Slimcc_Member *members;
  bool is_flexible;
  bool is_constructing;

  // Function parameter
  Slimcc_QualMask param_qual;

  // Function type
  Slimcc_Type *return_ty;
  Slimcc_Obj *param_list;
  bool is_variadic;
  bool is_oldstyle;
};

typedef struct {
  const char **data;
  int capacity;
  int len;
} Slimcc_StringArray;

typedef struct {
  bool err;
  int line;
  char *msg;
} Slimcc_Error;

typedef struct {
  Slimcc_StringArray include_paths;
  Slimcc_StringArray iquote_paths;
  Slimcc_StringArray display_files;
  bool opt_werror;
  Slimcc_StdVer opt_std;
  bool is_iso_std;
  bool opt_fdefer_ts;
  bool opt_short_enums;
  bool opt_gnu_keywords;
  bool opt_gnu89_inline;
  bool opt_ms_anon_struct;
} Slimcc_Options;

typedef struct {
  Slimcc_HashMap vars; // hm of Slimcc_VarScope
  Slimcc_HashMap tags; // hm of Slimcc_Type
  struct Slimcc_Ctx *ctx;
} Slimcc_GlobalDecls;

Slimcc_GlobalDecls slimcc_parse_global_decls(Slimcc_Options opts, Slimcc_Error *err);
void slimcc_free_global_decls(Slimcc_GlobalDecls *decls);

#endif