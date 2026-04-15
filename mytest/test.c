#include "../slimcc_lib.h"
#include <cgs/cgs.c>

CGS_StrView tok2view(Slimcc_Token *t)
{
    return (CGS_StrView){.chars = t->loc, .len = t->len};
}

struct [[Component]] FOO
{
    char *s;
    int i;
};

struct [[fjr::component, fjr::update_order(1, 2, 3)]] BAR
{
    char *s;
    int i;
};

[[fjr::one]]
int [[fjr::two]] play_sound() [[fjr::three]]
{
    
}

int foo(int x) {
    return x ?: 42;
}

[[fjr::x]] int [[fjr::y]] MYVAR [[fjr::z]] = 25 ? 25 : 25;

 [[deprecated]] typedef int  myi;
 
 
int main()
{
    struct FOO fff = ({
        int this_is_var = 25;
        struct FOO out = {.i = this_is_var};
        out;
    });
    
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
    for(int i = 0 ; i < ast.n_gtags ; i++)
    {
        Slimcc_Type *var = ast.gtags[i];
        if(var->tag)
        {
            CGS_StrView sv = tok2view(var->tag);
            if(cgs_equal(sv, "FOO") || cgs_equal(sv, "BAR"))
            {
                cgs_print("size == ", var->size, " :: ");
            }
            cgs_println(sv);
            c++;
        }
    }
    
    for(int i = 0 ; i < ast.n_gvars ; i++)
    {
        Slimcc_NamedVar var = ast.gvars[i];
        CGS_StrView sv = {.chars = var.name, .len = var.name_len};
        if(cgs_equal(sv, "myi"))
        {
            cgs_println(sv);
        }
    }
    
    slimcc_free_ast(&ast);
    printf("printed: %d\n", c);
    fflush(stdout);
}
