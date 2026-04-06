#include <cgs/cgs.c>
#include <stc/common.h>


uint64_t sv_hash(const CGS_StrView *sv)
{
    return c_hash_n(sv->chars, sv->len);
}

bool sv_equal(const CGS_StrView *a, const CGS_StrView *b)
{
    return cgs_equal(*a, *b);
}

#define i_eq sv_equal
#define i_hash sv_hash
#define T SVSet, CGS_StrView
#include <stc/hset.h>

int main(int argc, char **argv)
{
    if(argc != 4)
    {
        puts("usage error");
        exit(1);
    }
    
    SVSet funcs = SVSet_init();
    
    CGS_DStr dstr = cgs_dstr_init();
    
    FILE *f = fopen(argv[1], "r");
    cgs_fread_until(&dstr, f, EOF);
    fclose(f);
    
    CGS_StrView file_view = cgs_strv(dstr);
    
    CGS_DStr param = cgs_dstr_init_from("(");
    cgs_append(&param, argv[2]);
    
    CGS_DStr arg = cgs_dstr_init_from("(");
    cgs_append(&arg, argv[3]);
    
    CGS_DStr arg_comma_space = cgs_dstr_init_from(argv[3]);
    cgs_append(&arg_comma_space, ", ");
    
    while(file_view.len != 0)
    {
        cgs_next_tok(&file_view, param);
        if(file_view.len == 0)
        {
            break;
        }
        
        CGS_StrView func = {.chars = file_view.chars - param.len - 1, .len = 1};
        while(isalnum(func.chars[-1]) || func.chars[-1] == '_')
        {
            func.chars -= 1;
            func.len   += 1;
        }
        
        func = cgs_strv(cgs_dup(func));
        SVSet_push(&funcs, func);
    }
    
    
    for(c_each(it, SVSet, funcs))
    {
        file_view = cgs_strv(dstr);
        CGS_StrView func = *it.ref;
        
        while(file_view.len != 0)
        {
            cgs_next_tok(&file_view, func);
            if(file_view.len == 0)
                break;
            
            if(
                !cgs_starts_with(file_view, "(")  ||
                cgs_starts_with(file_view, param) ||
                cgs_starts_with(file_view, arg)
            )
            {
                continue;
            }
            
            file_view.chars += 1;
            file_view.len   -= 1;
            
            ptrdiff_t index = file_view.chars - dstr.chars;
            cgs_insert(&dstr, arg_comma_space, index);
        }
    }
    
    cgs_print(dstr);
}
