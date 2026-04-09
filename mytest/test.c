#include "../slimcc_lib.h"
#include <cgs/cgs.c>

constexpr float FLOAT = 10;

typedef struct [[Component]] FOO
{
    int i;
} FOO;

struct [[Component]] BAR
{
    int i;
};

CGS_StrView tok2view(Slimcc_Token *t)
{
    return (CGS_StrView){.chars = t->loc, .len = t->len};
}

int main()
{
    FILE *f = fopen("test.c", "r");
    CGS_DStr file_data = cgs_dstr_init();
    cgs_fread_until(&file_data, f, EOF);
    
    Slimcc_AST ast = slimcc_get_ast(2, (char*[]){"test", "-I/usr/lib/gcc/x86_64-linux-gnu/13/include/"}, "test.c", file_data.chars, 0);
    int c = 0;
    for(int i = 0 ; i < ast.n_gtags ; i++)
    {
        Slimcc_Type *type = ast.gtags[i];
        if(type && type->tag)
        {
            cgs_println(tok2view(type->tag));
            c++;
        }
    }
    cgs_println("printed: ", c);
}
