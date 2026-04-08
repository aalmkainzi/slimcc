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

int main()
{
    FILE *f = fopen("test.c", "r");
    CGS_DStr file_data = cgs_dstr_init();
    cgs_fread_until(&file_data, f, EOF);
    
    AST ast = slimcc_get_ast(2, (char*[]){"test", "-I/usr/lib/gcc/x86_64-linux-gnu/13/include/"}, "test.c", file_data.chars, 0);
    
    cgs_println("Printing all tags:");
    
    for(int i = 0 ; i < ast.n_gtags ; i++)
    {
        char *loc = ast.gtags[i]->tag->loc;
        if(loc) cgs_println(loc);
    }
}
