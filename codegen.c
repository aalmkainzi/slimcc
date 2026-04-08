#include "slimcc.h"

#define GP_MAX 6
#define FP_MAX 8

#define GP_SLOTS 6
#define FP_SLOTS 6

#define STRBUF_SZ 192
#define STRBUF_SZ2 208

typedef enum {
  REGSZ_8 = 0,
  REGSZ_16,
  REGSZ_32,
  REGSZ_64,
} RegSz;

typedef enum {
  REG_X64_NULL = 0,
  REG_X64_AX,
  REG_X64_CX,
  REG_X64_DX,
  REG_X64_SI,
  REG_X64_DI,
  REG_X64_R8,
  REG_X64_R9,
  REG_X64_R10,
  REG_X64_R11,
  REG_X64_R12,
  REG_X64_R13,
  REG_X64_R14,
  REG_X64_R15,
  REG_X64_BX,
  REG_X64_BP,
  REG_X64_SP,
  REG_X64_XMM0,
  REG_X64_XMM1,
  REG_X64_XMM2,
  REG_X64_XMM3,
  REG_X64_XMM4,
  REG_X64_XMM5,
  REG_X64_XMM6,
  REG_X64_XMM7,
  REG_X64_XMM8,
  REG_X64_XMM9,
  REG_X64_XMM10,
  REG_X64_XMM11,
  REG_X64_XMM12,
  REG_X64_XMM13,
  REG_X64_XMM14,
  REG_X64_XMM15,
  REG_X64_X87_ST0,
  REG_X64_X87_ST1,
  REG_X64_X87_ST2,
  REG_X64_X87_ST3,
  REG_X64_X87_ST4,
  REG_X64_X87_ST5,
  REG_X64_X87_ST6,
  REG_X64_X87_ST7,
  REG_X64_END
} Reg;

static char *regs[REG_X64_XMM0][4] = {
  [REG_X64_NULL] = {"null", "null", "null", "null"},
  [REG_X64_SP] = {"%spl", "%sp", "%esp", "%rsp"},
  [REG_X64_BP] = {"%bpl", "%bp", "%ebp", "%rbp"},
  [REG_X64_AX] = {"%al", "%ax", "%eax", "%rax"},
  [REG_X64_BX] = {"%bl", "%bx", "%ebx", "%rbx"},
  [REG_X64_CX] = {"%cl", "%cx", "%ecx", "%rcx"},
  [REG_X64_DX] = {"%dl", "%dx", "%edx", "%rdx"},
  [REG_X64_SI] = {"%sil", "%si", "%esi", "%rsi"},
  [REG_X64_DI] = {"%dil", "%di", "%edi", "%rdi"},
  [REG_X64_R8] = {"%r8b", "%r8w", "%r8d", "%r8"},
  [REG_X64_R9] = {"%r9b", "%r9w", "%r9d", "%r9"},
  [REG_X64_R10] = {"%r10b", "%r10w", "%r10d", "%r10"},
  [REG_X64_R11] = {"%r11b", "%r11w", "%r11d", "%r11"},
  [REG_X64_R12] = {"%r12b", "%r12w", "%r12d", "%r12"},
  [REG_X64_R13] = {"%r13b", "%r13w", "%r13d", "%r13"},
  [REG_X64_R14] = {"%r14b", "%r14w", "%r14d", "%r14"},
  [REG_X64_R15] = {"%r15b", "%r15w", "%r15d", "%r15"},
};

static char *argreg32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

static Reg argreg[] = {REG_X64_DI, REG_X64_SI, REG_X64_DX,
                       REG_X64_CX, REG_X64_R8, REG_X64_R9};

static char *tmpreg32[] = {"%edi", "%esi", "%r8d", "%r9d", "%r10d", "%r11d"};
static char *tmpreg64[] = {"%rdi", "%rsi", "%r8", "%r9", "%r10", "%r11"};

static char *rip = "%rip";
static char *rbp = "%rbp";
static char *rbx = "%rbx";

static Obj *codegen_fn;
static char *lvar_ptr;
static int va_gp_start;
static int va_fp_start;
static int va_st_start;
static int vla_base_ofs;
static int rtn_ptr_ofs;
static int lvar_stk_sz;
static int peak_stk_usage;
static int tmpbuf_sz;
static int64_t rtn_label;
static HashMap *ext_refs;
static bool *debug_file_used;
static int *debug_file_id;
static StringArray debug_files;

static struct {
  bool in[REG_X64_END];
  bool out[REG_X64_END];
} asm_use;

#define ASMOP_BUFSZ 32
static int asm_ops_cnt;
static AsmParam *asm_ops[ASMOP_BUFSZ];

static struct {
  char *rbp;
  char *rbx;
} asm_alt_ptr;

typedef enum {
  SL_GP,
  SL_FP,
  SL_ST,
} SlotKind;

typedef struct {
  SlotKind kind;
  int gp_depth;
  int fp_depth;
  int st_depth;
  int st_ofs;
  char *push_reg;
  long loc;
} Slot;

static struct {
  Slot *data;
  int capacity;
  int depth;
} tmp_stack;

struct AsmContext {
  Reg output_tmp1;
  Reg output_tmp2;
  Reg frame_ptr1;
  Reg frame_ptr2;
  uint32_t clobber_mask;
};

struct FuncObj {
  char *buf;
  size_t buflen;
  Obj **refs;
  int32_t ref_cnt;
  int32_t ref_capacity;
};

static void load2(Type *ty, int sofs, char *sptr);
static void store2(Type *ty, int dofs, char *dptr);
static void store_gp2(char **r, int sz, int dofs, char *dptr);

static void gen_asm(Node *node);
static void gen_expr(Node *node);
static void gen_stmt(Node *node);
static bool gen_block_stmt(Node *node, bool is_reach);
static bool gen_if_stmt(Node *node, bool is_reach);
static void gen_void_expr(Node *node);
static void gen_void_assign(Node *node);
static bool gen_reachable_stmt(Node *node);
static bool gen_unreachable_stmt(Node *node);
static bool gen_expr_opt(Node *node);
static bool gen_addr_opt(Node *node);
static bool gen_cmp_opt_gp(Node *node, NodeKind *kind);
static bool gen_load_opt_gp(Node *node, Reg r);
static Node *bool_expr_opt(Node *node, bool *flip, bool *is_void);
static void gen_mem_copy(char *sofs, char *sptr, char *dofs, char *dptr, int sz);
static void load_val2(Type *ty, int64_t val, char *gp32, char *gp64);

static void imm_add(char *op, char *tmp, int64_t val);
static void imm_sub(char *op, char *tmp, int64_t val);
static void imm_and(char *op, char *tmp, int64_t val);
static void imm_cmp(char *op, char *tmp, int64_t val);
static char *arith_ins(NodeKind kind);
static void imm_tmpl(char *ins, char *op, int64_t val);

static bool is_asm_symbolic_arg(Node *node, char *punct);

#define Prints(str) fprintf(output_file, str)
#define Printsts(str) fprintf(output_file, "\t" str)
#define Printssn(str) fprintf(output_file, str "\n")
#define Printstn(str) fprintf(output_file, "\t" str "\n")

#define Printf(str, ...) fprintf(output_file, str, __VA_ARGS__)
#define Printfts(str, ...) fprintf(output_file, "\t" str, __VA_ARGS__)
#define Printfsn(str, ...) fprintf(output_file, str "\n", __VA_ARGS__)
#define Printftn(str, ...) fprintf(output_file, "\t" str "\n", __VA_ARGS__)

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
int64_t align_to(int64_t n, int64_t align) {
  if (!align)
    internal_error();
  return (n + align - 1) / align * align;
}

enum { I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, F80 };


static char i32i8[] = "movsbl %al, %eax";
static char i32u8[] = "movzbl %al, %eax";
static char i32i16[] = "movswl %ax, %eax";
static char i32u16[] = "movzwl %ax, %eax";
static char i32f32[] = "cvtsi2ssl %eax, %xmm0";
static char i32i64[] = "movslq %eax, %rax";
static char i32f64[] = "cvtsi2sdl %eax, %xmm0";

static char u32f32[] = "mov %eax, %eax; cvtsi2ssq %rax, %xmm0";
static char u32i64[] = "mov %eax, %eax";
static char u32f64[] = "mov %eax, %eax; cvtsi2sdq %rax, %xmm0";

static char i64f32[] = "cvtsi2ssq %rax, %xmm0";
static char i64f64[] = "cvtsi2sdq %rax, %xmm0";

static char u64f32[] = "test %rax,%rax; js 1f; cvtsi2ss %rax,%xmm0; jmp 2f; "
                       "1: mov %rax,%rdx; and $1,%eax; shr $1, %rdx; "
                       "or %rax,%rdx; cvtsi2ss %rdx,%xmm0; addss %xmm0,%xmm0; 2:";
static char u64f64[] = "test %rax,%rax; js 1f; cvtsi2sd %rax,%xmm0; jmp 2f; "
                       "1: mov %rax,%rdx; and $1,%eax; shr $1, %rdx; "
                       "or %rax,%rdx; cvtsi2sd %rdx,%xmm0; addsd %xmm0,%xmm0; 2:";

static char f32i8[] = "cvttss2sil %xmm0, %eax; movsbl %al, %eax";
static char f32u8[] = "cvttss2sil %xmm0, %eax; movzbl %al, %eax";
static char f32i16[] = "cvttss2sil %xmm0, %eax; movswl %ax, %eax";
static char f32u16[] = "cvttss2sil %xmm0, %eax; movzwl %ax, %eax";
static char f32i32[] = "cvttss2sil %xmm0, %eax";
static char f32u32[] = "cvttss2siq %xmm0, %rax";
static char f32i64[] = "cvttss2siq %xmm0, %rax";
static char f32u64[] = "movl $0x5F000000, %eax; movd %eax, %xmm1; comiss %xmm0, %xmm1; "
                       "setbe %al; ja 1f; subss %xmm1, %xmm0; 1: "
                       "cvttss2siq %xmm0, %rdx; shlq $63, %rax; orq %rdx, %rax";
static char f32f64[] = "cvtss2sd %xmm0, %xmm0";

static char f64i8[] = "cvttsd2sil %xmm0, %eax; movsbl %al, %eax";
static char f64u8[] = "cvttsd2sil %xmm0, %eax; movzbl %al, %eax";
static char f64i16[] = "cvttsd2sil %xmm0, %eax; movswl %ax, %eax";
static char f64u16[] = "cvttsd2sil %xmm0, %eax; movzwl %ax, %eax";
static char f64i32[] = "cvttsd2sil %xmm0, %eax";
static char f64u32[] = "cvttsd2siq %xmm0, %rax";
static char f64i64[] = "cvttsd2siq %xmm0, %rax";
static char f64u64[] =
  "mov $0x43e0000000000000, %rax; movq %rax, %xmm1; comisd %xmm0, %xmm1; "
  "setbe %al; ja 1f; subsd %xmm1, %xmm0; 1: "
  "cvttsd2siq %xmm0, %rdx; shlq $63, %rax; orq %rdx, %rax";
static char f64f32[] = "cvtsd2ss %xmm0, %xmm0";

static char *cast_table[][10] = {
  // clang-format off
  // i8   i16     i32     i64     u8     u16     u32     u64     f32     f64
  {NULL,  NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i8
  {i32i8, NULL,   NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i16
  {i32i8, i32i16, NULL,   i32i64, i32u8, i32u16, NULL,   i32i64, i32f32, i32f64}, // i32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   i64f32, i64f64}, // i64

  {i32i8, NULL,   NULL,   i32i64, NULL,  NULL,   NULL,   i32i64, i32f32, i32f64}, // u8
  {i32i8, i32i16, NULL,   i32i64, i32u8, NULL,   NULL,   i32i64, i32f32, i32f64}, // u16
  {i32i8, i32i16, NULL,   u32i64, i32u8, i32u16, NULL,   u32i64, u32f32, u32f64}, // u32
  {i32i8, i32i16, NULL,   NULL,   i32u8, i32u16, NULL,   NULL,   u64f32, u64f64}, // u64

  {f32i8, f32i16, f32i32, f32i64, f32u8, f32u16, f32u32, f32u64, NULL,   f32f64}, // f32
  {f64i8, f64i16, f64i32, f64i64, f64u8, f64u16, f64u32, f64u64, f64f32, NULL  }  // f64
  // clang-format on
};





typedef enum {
  CLASS_NO,
  CLASS_INT,
  CLASS_SSE,
  CLASS_X87,
  CLASS_MEM,
} ArgClass;

static ArgClass calc_class(ArgClass a, ArgClass b) {
  if (a == b)
    return a;
  if (a == CLASS_NO || b == CLASS_NO)
    return a == CLASS_NO ? b : a;
  if (a == CLASS_MEM || b == CLASS_MEM)
    return CLASS_MEM;
  if (a == CLASS_X87 || b == CLASS_X87)
    return CLASS_MEM;
  if (a == CLASS_INT || b == CLASS_INT)
    return CLASS_INT;
  return CLASS_SSE;
}

static ArgClass get_class(Type *ty, int lo, int hi, int offset, ArgClass ac) {
  if (ty->kind == TY_STRUCT || ty->kind == TY_UNION) {
    for (Member *mem = ty->members; mem_iter(&mem); mem = mem->next) {
      int ofs = offset + mem->offset;
      if ((ofs + mem->ty->size) <= lo)
        continue;
      if (ofs >= hi)
        break;
      ac = get_class(mem->ty, lo, hi, ofs, ac);
    }
    return ac;
  }

  if (ty->kind == TY_ARRAY) {
    for (int i = 0; i < ty->array_len; i++) {
      int ofs = offset + ty->base->size * i;
      if ((ofs + ty->base->size) <= lo)
        continue;
      if (ofs >= hi)
        break;
      ac = get_class(ty->base, lo, hi, ofs, ac);
    }
    return ac;
  }

  switch (ty->kind) {
  case TY_FLOAT:
  case TY_DOUBLE:  return calc_class(ac, CLASS_SSE);
  case TY_LDOUBLE: return calc_class(ac, CLASS_X87);
  }
  return calc_class(ac, CLASS_INT);
}

static bool is_fp_class_lo(Type *ty) {
  return get_class(ty, 0, 8, 0, CLASS_NO) == CLASS_SSE;
}

static bool is_fp_class_hi(Type *ty) {
  return get_class(ty, 8, 16, 0, CLASS_NO) == CLASS_SSE;
}


static bool is_mem_class(Type *ty) {
  if (ty->size > 16)
    return true;
  return get_class(ty, 0, 16, 0, CLASS_NO) == CLASS_MEM;
}

static bool is_by_reg_agg(Type *ty) {
  if (ty->size > 16)
    return false;
  switch (get_class(ty, 0, 16, 0, CLASS_NO)) {
  case CLASS_X87:
  case CLASS_MEM: return false;
  }
  return true;
}



static int calling_convention(Obj *var, int *gp_count, int *fp_count, int *stack_align) {
  int stack = 0;
  int max_align = 16;
  int gp = *gp_count, fp = *fp_count;
  for (; var; var = var->param_next) {
    Type *ty = var->ty;
    assert(ty->size != 0);

    switch (ty->kind) {
    case TY_BITINT:
    case TY_STRUCT:
    case TY_UNION:
      if (is_by_reg_agg(ty)) {
        int fp_inc = is_fp_class_lo(ty) + (ty->size > 8 && is_fp_class_hi(ty));
        int gp_inc = !is_fp_class_lo(ty) + (ty->size > 8 && !is_fp_class_hi(ty));

        if ((!fp_inc || (fp + fp_inc <= FP_MAX)) && (!gp_inc || (gp + gp_inc <= GP_MAX))) {
          fp += fp_inc;
          gp += gp_inc;
          continue;
        }
      }
      break;
    case TY_FLOAT:
    case TY_DOUBLE:
      if (fp++ < FP_MAX)
        continue;
      break;
    case TY_LDOUBLE: {
      break;
    }
    default:
      if (gp++ < GP_MAX)
        continue;
    }
    var->pass_by_stack = true;

    if (ty->align > 8) {
      stack = align_to(stack, ty->align);
      max_align = MAX(max_align, ty->align);
    }
    var->stack_offset = stack;
    stack += align_to(ty->size, 8);
  }
  *gp_count = MIN(gp, GP_MAX);
  *fp_count = MIN(fp, FP_MAX);
  if (stack_align)
    *stack_align = max_align;

  return stack;
}

// Logic should be in sync with prepare_funcall()
void prepare_funcall(Node *node, Scope *scope) {
  bool rtn_by_stk = is_mem_class(node->ty);
  calling_convention(node->call.args, &(int){rtn_by_stk}, &(int){0}, NULL);

  int reg_arg_cnt = 0;
  for (Obj *var = node->call.args; var; var = var->param_next) {
    var->is_local = true;
    if (var->pass_by_stack) {
      var->ofs = var->stack_offset;
      var->ptr = "%rsp";
      continue;
    }
    var->next = scope->locals;
    scope->locals = var;
  }
}
