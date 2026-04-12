#include "../slimcc_lib.h"
#include <cgs/cgs.c>

CGS_StrView tok2view(Slimcc_Token *t)
{
    return (CGS_StrView){.chars = t->loc, .len = t->len};
}

struct [[Component]] FOO
{
    int i;
};

struct [[Component]] BAR
{
    int i;
};

int main()
{
    FILE *f = fopen("test.c", "r");
    CGS_DStr file_data = cgs_dstr_init();
    cgs_fread_until(&file_data, f, EOF);
#ifdef _WIN32
    #define AST_ARGS 3, (char*[]){"test", "-isystem", "C:/Users/aa.almkainzi/DevTools/w64devkit/include"}
#else
    #define AST_ARGS 2, (char*[]){"test", "-I/usr/lib/gcc/x86_64-linux-gnu/13/include/"}
#endif
    Slimcc_AST ast = slimcc_get_ast(AST_ARGS, "test.c", file_data.chars, 0);
    int c = 0;
    for(int i = 0 ; i < ast.n_gvars ; i++)
    {
        Slimcc_NamedVar var = ast.gvars[i];
        if(var.var->type_def)
        {
            CGS_StrView sv = (CGS_StrView){.chars = var.name, .len = var.name_len};
            cgs_println(sv);
            c++;
        }
    }
    
    slimcc_free_ast(&ast);
    printf("printed: %d\n", c);
    fflush(stdout);
}
