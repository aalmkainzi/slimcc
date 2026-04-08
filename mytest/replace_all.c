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

bool strv_in_dstr_arr(CGS_StrView elm, CGS_DStr *arr, size_t n);

int main(int argc, char **argv)
{
    if(argc < 4)
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
    
    size_t narg_variants = (argc - 3);
    CGS_DStr *args = malloc(sizeof(*args) * narg_variants);
    CGS_DStr *args_comma_space = malloc(sizeof(*args_comma_space) * narg_variants);
    for(int i = 3 ; i < argc ; i++)
    {
        args[i - 3] = cgs_dstr_init_from("(");
        cgs_append(&args[i - 3], argv[i]);
        
        args_comma_space[i - 3] = cgs_dstr_init_from(argv[i]);
        cgs_append(&args_comma_space[i - 3], ", ");
    }
    
    
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
                strv_in_dstr_arr(file_view, args, narg_variants)
            )
            {
                continue;
            }
            
            CGS_StrView found_func = file_view;
            found_func.chars -= 1;
            found_func.len = 1;
            
            while(isalnum(found_func.chars[-1]) || found_func.chars[-1] == '_')
            {
                found_func.chars -= 1;
                found_func.len += 1;
            }
            
            if(!cgs_equal(found_func, func))
                continue;
            
            file_view.chars += 1;
            file_view.len   -= 1;
            
            ptrdiff_t index = file_view.chars - dstr.chars;
            cgs_insert(&dstr, args_comma_space[0], index);
        }
    }
    
    cgs_print(dstr);
}

bool strv_in_dstr_arr(CGS_StrView elm, CGS_DStr *arr, size_t n)
{
    for(size_t i = 0 ; i < n ; i++)
    {
        if(cgs_starts_with(elm, arr[i]))
        {
            return true;
        }
    }
    return false;
}