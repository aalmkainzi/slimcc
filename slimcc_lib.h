#ifndef SLIMCC_LIB_H
#define SLIMCC_LIB_H

#include <stdint.h>
#include <stdbool.h>

#if __STDC_VERSION__ >= 201112L
# define ANON_UNION_START union {
# define ANON_UNION_END \
   }                    \
   ;
#else
# define ANON_UNION_START
# define ANON_UNION_END
#endif

#ifdef NO_LONG_DOUBLE
typedef double long_double_t;
#else
typedef long double long_double_t;
#endif

typedef struct Slimcc_AST Slimcc_AST;
typedef struct Slimcc_Type Slimcc_Type;
typedef struct Slimcc_Obj Slimcc_Obj;
typedef struct Slimcc_Node Slimcc_Node;
typedef struct Slimcc_Relocation Slimcc_Relocation;
typedef struct Slimcc_Member Slimcc_Member;
typedef struct Slimcc_Token Slimcc_Token;
typedef struct Slimcc_EnumVal Slimcc_EnumVal;
typedef struct Slimcc_File Slimcc_File;

typedef struct Slimcc_VarScope {
    Slimcc_Obj *var;
    Slimcc_Type *type_def;
    Slimcc_Type *enum_ty;
    int64_t enum_val;
    int32_t type_def_align;
    
    Slimcc_Token *name;
    Slimcc_Token *begin;
    Slimcc_Token *end;
} Slimcc_VarScope;

typedef struct Slimcc_NamedVar
{
  char *name;
  int name_len;
  Slimcc_VarScope *var;
} Slimcc_NamedVar;

typedef enum {
    INCL_ABS = -2,
    INCL_REL = -1,
} Slimcc_InclIdx;

struct Slimcc_File {
    char *name;
    char *contents;
    int file_no;
    
    int display_file_no;
    int line_delta;
    Slimcc_InclIdx incl_idx;
    bool is_syshdr;
};

typedef enum {
    TK_IDENT,   // Identifiers
    TK_PUNCT,   // Punctuators
    TK_KEYWORD, // Keywords
    TK_STR,     // String literals
    TK_ASM_STR,
    TK_INT_NUM, // Integer Numeric literals
    TK_PP_NUM,  // Preprocessing numbers
    TK_FMARK,   // Filemarkers for -E
    TK_PMARK,   // Placermarkers
    TK_ATTR,    // GNU attribute
    TK_BATTR,   // C23 attribute
    TK_EOF,     // End-of-file markers
    TK_UNICODE,
    
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

struct Slimcc_EnumVal {
    Slimcc_EnumVal *next;
    Slimcc_Token *name;
    int64_t val;
};

typedef enum {
    Q_CONST = 1,
    Q_VOLATILE = 1 << 1,
    Q_ATOMIC = 1 << 2,
    Q_RESTRICT = 1 << 3,
} Slimcc_QualMask;

// Slimcc_AST node
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
    ND_CAST,      // Slimcc_Type cast
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
} Slimcc_NodeKind;

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

// Struct member
struct Slimcc_Member {
    Slimcc_Member *next;
    Slimcc_Type *ty;
    
    Slimcc_Token *name;
    Slimcc_Token *begin;
    Slimcc_Token *end;
    
    int64_t offset;
    int idx;
    int alt_align;
    bool is_packed;
    
    bool is_bitfield;
    bool is_aligned_bitfield;
    int bit_offset;
    int bit_width;
};

struct Slimcc_Relocation {
    Slimcc_Relocation *next;
    int offset;
    char **label;
    Slimcc_Obj *var;
    long addend;
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
  
  // Slimcc_Token *tyspec_tok;
  // Slimcc_Token *name_tok;
  // Slimcc_Token *tok;
  
  // Local variable
  int ofs;
  char *ptr;
  Slimcc_Obj *param_next;
  bool pass_by_stack;
  int stack_offset;
  Slimcc_Node *arg_expr;
  Slimcc_Obj *param_promoted;
  
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
  Slimcc_Relocation *rel;
  
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
  Slimcc_Node *body;
  struct Slimcc_FuncObj *output; // backend defined output object
};

struct Slimcc_Type {
  Slimcc_TypeKind kind;
  int64_t size;
  int32_t align;
  bool is_unsigned;
  bool is_int_enum;
  bool is_enum;
  Slimcc_QualMask qual;
  Slimcc_Type *origin;
  Slimcc_Type *decl_next; // forward declarations
  Slimcc_Token *tag;
  Slimcc_Token *kw; // points to "struct", "union", "enum"
  Slimcc_EnumVal *enums;

  // Pointer-to or array-of type.
  Slimcc_Type *base;

  // _BitInt
  int64_t bit_cnt;

  // Array
  int64_t array_len;

  // Variable-length array
  Slimcc_Node *vla_len_expr;
  Slimcc_Obj *vla_len_val;

  // Struct
  Slimcc_Member *members;
  bool is_flexible;
  bool is_constructing;

  // Function parameter
  Slimcc_QualMask param_qual;

  // Function type
  struct Slimcc_Scope *scopes;
  Slimcc_Type *return_ty;
  Slimcc_Obj *param_list;
  Slimcc_Node *pre_calc;
  bool is_variadic;
  bool is_oldstyle;
};

// Slimcc_AST node type
struct Slimcc_Node {
  Slimcc_Node *next;
  Slimcc_NodeKind kind;
  Slimcc_NodeKind arith_kind : 30; // Arithmetic Assignment
  bool no_label : 1;
  bool is_nonlval : 1;
  Slimcc_Type *ty;
  Slimcc_Token *tok; // Representative token

  struct Slimcc_DeferStmt *dfr_from;
  struct Slimcc_DeferStmt *dfr_dest;

  ANON_UNION_START
  // Misc
  struct {
    Slimcc_Node *lhs;
    Slimcc_Node *rhs;
    Slimcc_Node *target;
    Slimcc_Obj *var;
    Slimcc_Member *member;
  } m;

  // Numeric literal
  struct {
    int64_t val;
    uint64_t *bitint_data;
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
    Slimcc_Node *body;
    Slimcc_Node *local_labels;
    Slimcc_Node *result;
  } blk;

  // if, ?:, for, do, while, switch
  struct {
    Slimcc_Node *cond;
    Slimcc_Node *then;
    ANON_UNION_START
    Slimcc_Node *els;
    Slimcc_Node *for_init;
    Slimcc_Node *sw_default;
    ANON_UNION_END
    ANON_UNION_START
    Slimcc_Node *for_inc;
    struct Slimcc_CaseRange *sw_cases;
    ANON_UNION_END
    Slimcc_Node *breaks;
  } ctrl;

  // goto, break, continue, case, labels
  struct {
    Slimcc_Node *next;
    Slimcc_Node *node;
    char *unique_label;
  } lbl;

  // Function call
  struct {
    Slimcc_Node *expr;
    Slimcc_Obj *rtn_buf;
    Slimcc_Obj *args;
  } call;

  // Atomic compare-and-swap
  struct {
    Slimcc_Node *addr;
    Slimcc_Node *old_val;
    Slimcc_Node *new_val;
  } cas;

  // GNU inline assembly
  struct {
    Slimcc_Token *str_tok;
    struct Slimcc_AsmParam *outputs;
    struct Slimcc_AsmParam *inputs;
    Slimcc_Token *clobbers;
    struct Slimcc_AsmParam *labels;
    struct AsmContext *ctx; // backend defined
  } gasm;
  ANON_UNION_END
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
    int len;   // Slimcc_Token length
    char *loc; // Slimcc_Token location
    struct Slimcc_File *file;
    Slimcc_Token *origin; // If this is expanded from a macro, the original token
    int line_no;   // Line number
    int display_line_no;
    int display_file_no;
    Slimcc_Type *ty; // Used if TK_INT_NUM or TK_STR

    Slimcc_Token *attr_vendor;
    bool attr_supported;

    ANON_UNION_START
    Slimcc_Token *attr_next;
    Slimcc_Token *alloc_next;
    ANON_UNION_END
    ANON_UNION_START
    int64_t ival; // If kind is TK_INT_NUM, its value
    char *str;    // String literal contents including terminating '\0'
    Slimcc_Token *label_next;
    ANON_UNION_END
};

typedef struct Slimcc_AST
{
    Slimcc_Obj *objects;
    
    int n_gvars;
    Slimcc_NamedVar *gvars;
    
    int n_gtags;
    Slimcc_Type **gtags;
    
    void *ctx;
} Slimcc_AST;

struct SlimccReport;
Slimcc_AST slimcc_get_ast(int argc, const char *const*argv, const char *file_name, char *source_data, struct SlimccReport *report);
void slimcc_free_ast(Slimcc_AST *ast);

#endif